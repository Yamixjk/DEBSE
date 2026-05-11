"""
Aegnis - Backend Flask para Raspberry Pi
Rutas:
  GET  /                      → interfaz de configuración
  GET  /api/wifi/scan         → escanea redes WiFi disponibles
  POST /api/wifi/connect      → conecta a una red WiFi
  GET  /api/telegram/code     → genera código de vinculación
  GET  /api/telegram/verify   → verifica si alguien se vinculó
  GET  /api/status            → estado general del sistema
  POST /api/owner/remove      → elimina al propietario actual
  GET  /api/usuarios          → lista padre e hijos
  POST /api/hijo/remove       → elimina un hijo
"""

import os
import json
import time
import random
import string
import threading
import subprocess
from pathlib import Path
from flask import Flask, jsonify, request, send_from_directory, Response

BASE_DIR    = Path(__file__).parent
CONFIG_FILE = BASE_DIR / 'config.json'
STATIC_DIR  = BASE_DIR / 'static'
TOKEN_BOT   = "8520152162:AAEMYYcvky9gMfiw_BX-XMZ960pEFU0IpRw"
FRAME_STREAM = Path("/tmp/aegnis_frame.jpg")

app = Flask(__name__, static_folder=str(STATIC_DIR))

estado = {
    "codigo_vinculacion": None,
    "codigo_expira":      0,
    "codigo_para":        None,   # 'padre' o chat_id del padre que invitó
    "pendientes":         {},     # chat_id → {codigo, expira, nombre}
    "verificado":         False,
}

# ─── Config ───────────────────────────────────────────────────────

def cargar_config():
    if CONFIG_FILE.exists():
        with open(CONFIG_FILE) as f:
            data = json.load(f)
        if 'hijos' not in data:
            data['hijos'] = []
        return data
    return {"wifi_ssid": "", "chat_id": "", "owner_vinculado": False, "hijos": []}

def guardar_config(data: dict):
    with open(CONFIG_FILE, 'w') as f:
        json.dump(data, f, indent=2)

def todos_los_chat_ids():
    """Devuelve lista con el chat_id del padre y todos los hijos."""
    config = cargar_config()
    ids = []
    if config.get('chat_id'):
        ids.append(config['chat_id'])
    for hijo in config.get('hijos', []):
        if hijo.get('chat_id'):
            ids.append(hijo['chat_id'])
    return ids

def wifi_actual():
    """Devuelve el SSID de la red WiFi actualmente conectada."""
    try:
        result = subprocess.run(
            ['nmcli', '-t', '-f', 'ACTIVE,SSID', 'dev', 'wifi'],
            capture_output=True, text=True, timeout=10
        )
        for line in result.stdout.strip().split('\n'):
            parts = line.split(':')
            if len(parts) >= 2 and parts[0] == 'yes':
                return parts[1].strip()
    except Exception:
        pass
    return None

def ip_local():
    """Devuelve la IP local actual de la Pi."""
    try:
        result = subprocess.run(
            ['hostname', '-I'],
            capture_output=True, text=True, timeout=5
        )
        ips = result.stdout.strip().split()
        for ip in ips:
            if ip.startswith('192.168') or ip.startswith('10.'):
                return ip
        return ips[0] if ips else '?.?.?.?'
    except Exception:
        return '?.?.?.?'

def escanear_redes():
    """Devuelve lista de redes WiFi disponibles."""
    try:
        result = subprocess.run(
            ['nmcli', '-t', '-f', 'SSID,SIGNAL,SECURITY', 'dev', 'wifi', 'list', '--rescan', 'yes'],
            capture_output=True, text=True, timeout=15
        )
        redes = []
        seen  = set()
        for line in result.stdout.strip().split('\n'):
            parts = line.split(':')
            if len(parts) < 2:
                continue
            ssid = parts[0].strip()
            if ssid and ssid not in seen and ssid != 'Aegnis-Setup':
                seen.add(ssid)
                redes.append(ssid)
        return redes
    except Exception:
        return []


def generar_codigo(largo=6):
    chars = string.ascii_uppercase + string.digits
    return ''.join(random.choices(chars, k=largo))

def enviar_telegram(chat_id, texto, parse_mode='Markdown'):
    import requests as req
    try:
        req.post(
            f"https://api.telegram.org/bot{TOKEN_BOT}/sendMessage",
            json={'chat_id': chat_id, 'text': texto, 'parse_mode': parse_mode},
            timeout=10
        )
    except Exception as e:
        print(f"⚠️  Error enviando mensaje a {chat_id}: {e}")

def notificar_todos(texto):
    for cid in todos_los_chat_ids():
        enviar_telegram(cid, texto)

# ─── Rutas estáticas ──────────────────────────────────────────────

@app.route('/')
def index():
    return send_from_directory(str(BASE_DIR), 'rpi-setup.html')

# ─── Stream de video ──────────────────────────────────────────────

def generar_stream():
    while True:
        try:
            if FRAME_STREAM.exists():
                with open(FRAME_STREAM, 'rb') as f:
                    frame = f.read()
                yield (b'--frame\r\n'
                       b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')
        except Exception:
            pass
        time.sleep(0.1)

@app.route('/video')
def video_stream():
    return Response(generar_stream(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/monitor')
def monitor():
    config = cargar_config()
    ssid   = wifi_actual() or config.get('wifi_ssid', '—')
    html = f"""<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Aegnis — Monitor</title>
  <style>
    * {{ box-sizing: border-box; margin: 0; padding: 0; }}
    body {{ background: #0f0f0f; color: #eee; font-family: sans-serif; padding: 16px; }}
    h1 {{ font-size: 1.3rem; margin-bottom: 12px; color: #ff6b35; }}
    .badge {{ display: inline-block; padding: 4px 10px; border-radius: 12px;
              font-size: 0.8rem; background: #222; margin-bottom: 16px; }}
    img {{ width: 100%; max-width: 640px; border-radius: 8px;
           border: 2px solid #333; display: block; }}
    .info {{ margin-top: 12px; font-size: 0.85rem; color: #aaa; }}
  </style>
</head>
<body>
  <h1>🔥 Aegnis — Monitor en vivo</h1>
  <div class="badge">📡 {ssid}</div>
  <img src="/video" alt="Stream de cámara">
  <div class="info">Las detecciones se actualizan en tiempo real.</div>
</body>
</html>"""
    return html

# ─── WiFi ─────────────────────────────────────────────────────────

@app.route('/api/wifi/scan')
def wifi_scan():
    try:
        result = subprocess.run(
            ['nmcli', '-t', '-f', 'SSID,SIGNAL,SECURITY', 'dev', 'wifi', 'list', '--rescan', 'yes'],
            capture_output=True, text=True, timeout=15
        )
        redes = []
        seen  = set()
        for line in result.stdout.strip().split('\n'):
            parts = line.split(':')
            if len(parts) < 2:
                continue
            ssid   = parts[0].strip()
            signal = int(parts[1]) if parts[1].isdigit() else 0
            lock   = len(parts) > 2 and parts[2].strip() not in ('', '--')
            if ssid and ssid not in seen:
                seen.add(ssid)
                nivel = 'strong' if signal >= 70 else 'medium' if signal >= 40 else 'weak'
                redes.append({'ssid': ssid, 'signal': nivel, 'lock': lock})
        redes.sort(key=lambda r: {'strong': 0, 'medium': 1, 'weak': 2}[r['signal']])
        return jsonify({'ok': True, 'redes': redes})
    except Exception as e:
        return jsonify({'ok': False, 'error': str(e)}), 500


@app.route('/api/wifi/connect', methods=['POST'])
def wifi_connect():
    data = request.get_json()
    ssid = (data or {}).get('ssid', '').strip()
    pwd  = (data or {}).get('pass', '').strip()

    if not ssid:
        return jsonify({'ok': False, 'error': 'SSID requerido'}), 400

    def conectar_en_hilo():
        try:
            if pwd:
                cmd = ['nmcli', 'dev', 'wifi', 'connect', ssid, 'password', pwd]
            else:
                cmd = ['nmcli', 'dev', 'wifi', 'connect', ssid]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            if result.returncode == 0:
                config = cargar_config()
                config['wifi_ssid'] = ssid
                guardar_config(config)
                print(f"✅ Conectado a {ssid}")
            else:
                print(f"❌ Error conectando a {ssid}: {result.stderr}")
        except Exception as e:
            print(f"❌ Excepción conectando WiFi: {e}")

    # Responder antes de que la conexión corte el hilo actual
    hilo = threading.Thread(target=conectar_en_hilo, daemon=True)
    hilo.start()

    config = cargar_config()
    config['wifi_ssid'] = ssid
    guardar_config(config)

    return jsonify({'ok': True, 'mensaje': f'Conectando a {ssid}...'})

# ─── Telegram ─────────────────────────────────────────────────────

@app.route('/api/telegram/code')
def telegram_code():
    codigo = generar_codigo()
    estado['codigo_vinculacion'] = codigo
    estado['codigo_expira']      = time.time() + 300
    estado['codigo_para']        = 'padre'
    estado['verificado']         = False
    return jsonify({'ok': True, 'codigo': codigo, 'expira_en': 300})


@app.route('/api/telegram/verify')
def telegram_verify():
    if time.time() > estado.get('codigo_expira', 0):
        return jsonify({'ok': False, 'error': 'Código expirado'}), 410

    config = cargar_config()
    if config.get('owner_vinculado') and config.get('chat_id'):
        estado['verificado'] = True
        return jsonify({'ok': True, 'chat_id': config['chat_id']})

    return jsonify({'ok': False, 'pendiente': True,
                    'mensaje': 'Aún no se ha recibido el código en Telegram'})


@app.route('/api/owner/remove', methods=['POST'])
def owner_remove():
    data = request.get_json()
    if not (data or {}).get('confirmar'):
        return jsonify({'ok': False, 'error': 'Se requiere confirmación'}), 400

    config = cargar_config()
    config['chat_id']         = ''
    config['owner_vinculado'] = False
    config['hijos']           = []
    guardar_config(config)
    estado['verificado'] = False
    return jsonify({'ok': True, 'mensaje': 'Propietario eliminado'})


@app.route('/api/usuarios')
def listar_usuarios():
    config = cargar_config()
    return jsonify({
        'ok':    True,
        'padre': config.get('chat_id', ''),
        'hijos': config.get('hijos', [])
    })


@app.route('/api/hijo/remove', methods=['POST'])
def hijo_remove():
    data    = request.get_json()
    chat_id = (data or {}).get('chat_id', '')
    if not chat_id:
        return jsonify({'ok': False, 'error': 'chat_id requerido'}), 400

    config = cargar_config()
    antes  = len(config.get('hijos', []))
    config['hijos'] = [h for h in config.get('hijos', []) if h['chat_id'] != chat_id]
    if len(config['hijos']) == antes:
        return jsonify({'ok': False, 'error': 'Usuario no encontrado'}), 404

    guardar_config(config)
    enviar_telegram(chat_id, '❌ Has sido eliminado de FireGuard. Ya no recibirás alertas.')
    return jsonify({'ok': True})

# ─── Estado ───────────────────────────────────────────────────────

@app.route('/api/status')
def system_status():
    config = cargar_config()
    try:
        subprocess.run(['ping', '-c', '1', '-W', '2', '8.8.8.8'],
                       capture_output=True, timeout=5)
        internet = True
    except Exception:
        internet = False

    modelo_path = BASE_DIR / 'best.onnx'
    modelo_ok   = modelo_path.exists()

    return jsonify({
        'ok':              True,
        'wifi_ssid':       config.get('wifi_ssid', ''),
        'internet':        internet,
        'owner_vinculado': config.get('owner_vinculado', False),
        'chat_id':         config.get('chat_id', ''),
        'hijos':           config.get('hijos', []),
        'modelo_ok':       modelo_ok,
        'modelo_path':     str(modelo_path),
    })

# ─── Bot de Telegram ──────────────────────────────────────────────

def bot_loop():
    import requests as req

    url         = f"https://api.telegram.org/bot{TOKEN_BOT}"
    last_update = 0

    print("🤖 Bot de Telegram iniciado.")

    while True:
        try:
            resp = req.get(f"{url}/getUpdates",
                           params={'offset': last_update + 1, 'timeout': 10},
                           timeout=15)
            updates = resp.json().get('result', [])

            for upd in updates:
                last_update = upd['update_id']
                msg         = upd.get('message', {})
                chat_id     = str(msg.get('chat', {}).get('id', ''))
                nombre      = msg.get('chat', {}).get('first_name', 'Usuario')
                texto       = msg.get('text', '').strip()

                if not chat_id or not texto:
                    continue

                config   = cargar_config()
                es_padre = config.get('chat_id') == chat_id

                # ── /start ──
                if texto == '/start':
                    enviar_telegram(chat_id,
                        '🔥 *Bienvenido a Aegnis*\n\n'
                        'Comandos disponibles:\n'
                        '• `/vincular` — Vincular este número al sistema\n'
                        '• `/estado` — Ver estado del sistema\n'
                        '• `/monitor` — Ver video en vivo con detecciones\n'
                        '• `/red` — Cambiar red WiFi _(solo padre)_\n'
                        '• `/usuarios` — Ver usuarios registrados _(solo padre)_\n'
                        '• `/agregar` — Generar código para un hijo _(solo padre)_\n'
                        '• `/eliminar` — Eliminar un hijo _(solo padre)_'
                    )

                # ── /monitor ──
                elif texto == '/monitor':
                    if not (es_padre or any(h['chat_id'] == chat_id for h in config.get('hijos', []))):
                        enviar_telegram(chat_id, '⛔ No estás vinculado a este sistema.')
                        continue
                    ip   = ip_local()
                    ssid = wifi_actual() or config.get('wifi_ssid', '—')
                    enviar_telegram(chat_id,
                        f'📷 *Monitor en vivo*\n\n'
                        f'Conéctate a la misma red WiFi que la Pi y abre:\n\n'
                        f'`http://{ip}/monitor`\n\n'
                        f'o también:\n'
                        f'`http://fisica1.local/monitor`\n\n'
                        f'📡 Red actual: *{ssid}*'
                    )

                # ── /vincular ──
                elif texto == '/vincular':
                    if config.get('owner_vinculado') and not es_padre:
                        ya_hijo = any(h['chat_id'] == chat_id for h in config.get('hijos', []))
                        if ya_hijo:
                            enviar_telegram(chat_id, '✅ Ya estás vinculado como hijo en este sistema.')
                        else:
                            enviar_telegram(chat_id,
                                '🔐 *Para unirte a Aegnis:*\n\n'
                                '1. Pide al padre que use `/agregar` en este bot\n'
                                '2. Te compartirá un código de 6 caracteres\n'
                                '3. Manda ese código aquí directamente\n\n'
                                '_Ejemplo: manda_ `ABC123`'
                            )
                    elif not config.get('owner_vinculado'):
                        codigo = estado.get('codigo_vinculacion', '—')
                        if not codigo or time.time() > estado.get('codigo_expira', 0):
                            enviar_telegram(chat_id,
                                '⚠️ No hay código activo. Genera uno desde la interfaz de configuración.')
                        else:
                            enviar_telegram(chat_id,
                                f'🔐 Ingresa el código de vinculación que aparece en la pantalla:\n\n'
                                f'`{codigo}`\n\nExpira en 5 minutos.',
                            )
                    else:
                        enviar_telegram(chat_id, '✅ Ya eres el padre de este sistema.')

                # ── /estado ──
                elif texto == '/estado':
                    if not (es_padre or any(h['chat_id'] == chat_id for h in config.get('hijos', []))):
                        enviar_telegram(chat_id, '⛔ No estás vinculado a este sistema.')
                        continue
                    try:
                        subprocess.run(['ping', '-c', '1', '-W', '2', '8.8.8.8'],
                                       capture_output=True, timeout=5)
                        internet = '✅ Conectado'
                    except Exception:
                        internet = '❌ Sin conexión'

                    red_real  = wifi_actual() or config.get('wifi_ssid', '—')
                    modelo_ok = (BASE_DIR / 'best.onnx').exists()
                    enviar_telegram(chat_id,
                        f'📊 *Estado de Aegnis*\n\n'
                        f'🌐 Internet: {internet}\n'
                        f'📡 WiFi: {red_real}\n'
                        f'🤖 Modelo: {"✅ Cargado" if modelo_ok else "❌ No encontrado"}\n'
                        f'👤 Padre: {"✅ Vinculado" if config.get("owner_vinculado") else "❌ Sin vincular"}\n'
                        f'👨‍👩‍👧 Hijos: {len(config.get("hijos", []))}'
                    )

                # ── /red (solo padre) ──
                elif texto == '/red':
                    if not es_padre:
                        enviar_telegram(chat_id, '⛔ Solo el padre puede cambiar la red WiFi.')
                        continue
                    redes = escanear_redes()
                    if not redes:
                        enviar_telegram(chat_id, '⚠️ No se encontraron redes disponibles. Intenta de nuevo.')
                    else:
                        lista = '\n'.join([f'{i+1}. {r}' for i, r in enumerate(redes)])
                        estado[f'redes_disponibles_{chat_id}'] = redes
                        estado[f'esperando_red_{chat_id}'] = True
                        enviar_telegram(chat_id,
                            f'📶 *Redes WiFi disponibles:*\n\n{lista}\n\n'
                            f'Responde con el *número* de la red a la que quieres conectarte.'
                        )

                # ── Selección de red por número ──
                elif estado.get(f'esperando_red_{chat_id}') and es_padre:
                    redes = estado.get(f'redes_disponibles_{chat_id}', [])
                    if texto.isdigit() and 1 <= int(texto) <= len(redes):
                        ssid_elegido = redes[int(texto) - 1]
                        del estado[f'esperando_red_{chat_id}']
                        del estado[f'redes_disponibles_{chat_id}']
                        estado[f'esperando_pass_{chat_id}'] = ssid_elegido
                        enviar_telegram(chat_id,
                            f'🔒 Escribe la contraseña de *{ssid_elegido}*:\n\n'
                            f'_(Si no tiene contraseña, escribe `sin contraseña`)_'
                        )
                    else:
                        enviar_telegram(chat_id, '⚠️ Responde con el número de la red de la lista.')

                # ── Contraseña para nueva red ──
                elif estado.get(f'esperando_pass_{chat_id}') and es_padre:
                    ssid_nuevo = estado[f'esperando_pass_{chat_id}']
                    password   = '' if texto.lower() == 'sin contraseña' else texto
                    del estado[f'esperando_pass_{chat_id}']

                    enviar_telegram(chat_id, f'⏳ Conectando a *{ssid_nuevo}*...')

                    def cambiar_red(ssid, pwd, cid):
                        try:
                            if pwd:
                                cmd = ['nmcli', 'connection', 'add', 'type', 'wifi',
                                       'ssid', ssid, 'wifi-sec.key-mgmt', 'wpa-psk',
                                       'wifi-sec.psk', pwd, 'connection.id', f'aegnis-{ssid}',
                                       'ifname', 'wlan0']
                            else:
                                cmd = ['nmcli', 'connection', 'add', 'type', 'wifi',
                                       'ssid', ssid, 'connection.id', f'aegnis-{ssid}',
                                       'ifname', 'wlan0']
                            subprocess.run(cmd, capture_output=True, timeout=15)
                            result = subprocess.run(
                                ['nmcli', 'connection', 'up', f'aegnis-{ssid}'],
                                capture_output=True, text=True, timeout=30
                            )
                            if result.returncode == 0:
                                cfg = cargar_config()
                                cfg['wifi_ssid'] = ssid
                                guardar_config(cfg)
                                time.sleep(3)
                                enviar_telegram(cid, f'✅ Conectado a *{ssid}* correctamente.')
                            else:
                                enviar_telegram(cid, f'❌ No se pudo conectar a *{ssid}*. Verifica la contraseña.')
                        except Exception as e:
                            enviar_telegram(cid, f'❌ Error al cambiar red: {e}')

                    hilo = threading.Thread(target=cambiar_red, args=(ssid_nuevo, password, chat_id), daemon=True)
                    hilo.start()

                # ── /usuarios (solo padre) ──
                elif texto == '/usuarios':
                    if not es_padre:
                        enviar_telegram(chat_id, '⛔ Solo el padre puede ver los usuarios.')
                        continue
                    hijos = config.get('hijos', [])
                    if not hijos:
                        enviar_telegram(chat_id, '👤 Solo tú (padre) estás vinculado.\n\nUsa `/agregar` para invitar a alguien.')
                    else:
                        lista = '\n'.join([f"• {h.get('nombre','?')} (`{h['chat_id']}`)" for h in hijos])
                        enviar_telegram(chat_id, f'👨‍👩‍👧 *Usuarios vinculados:*\n\n{lista}')

                # ── /agregar (solo padre) ──
                elif texto == '/agregar':
                    if not es_padre:
                        enviar_telegram(chat_id, '⛔ Solo el padre puede agregar usuarios.')
                        continue
                    codigo  = generar_codigo()
                    expira  = time.time() + 600   # 10 minutos
                    # Guardar código pendiente general (cualquier hijo puede usarlo)
                    if 'pendientes_hijo' not in estado:
                        estado['pendientes_hijo'] = {}
                    estado['pendientes_hijo'][codigo] = {'expira': expira}
                    enviar_telegram(chat_id,
                        f'🔑 *Código para nuevo hijo:*\n\n`{codigo}`\n\n'
                        f'Compártelo con la persona que quieres agregar.\n'
                        f'Deben abrir el bot @incendios_alerta_bot y mandar este código.\n'
                        f'_Expira en 10 minutos._'
                    )

                # ── /eliminar (solo padre) ──
                elif texto == '/eliminar':
                    if not es_padre:
                        enviar_telegram(chat_id, '⛔ Solo el padre puede eliminar usuarios.')
                        continue
                    hijos = config.get('hijos', [])
                    if not hijos:
                        enviar_telegram(chat_id, '⚠️ No hay hijos vinculados.')
                    else:
                        lista = '\n'.join([f"• {h.get('nombre','?')} — escribe su número: `{h['chat_id']}`" for h in hijos])
                        enviar_telegram(chat_id,
                            f'Para eliminar un hijo, responde con su chat ID:\n\n{lista}\n\n'
                            f'_Ejemplo: responde solo con el número_'
                        )
                        estado[f'esperando_eliminar_{chat_id}'] = True

                # ── Código de vinculación del PADRE (setup inicial) ──
                elif (not config.get('owner_vinculado')
                      and texto.upper() == estado.get('codigo_vinculacion', '')
                      and time.time() < estado.get('codigo_expira', 0)):

                    config['chat_id']         = chat_id
                    config['owner_vinculado'] = True
                    config['hijos']           = []
                    guardar_config(config)
                    estado['codigo_vinculacion'] = None

                    enviar_telegram(chat_id,
                        '✅ *Vinculación exitosa. Eres el padre de FireGuard.*\n\n'
                        'A partir de ahora recibirás todas las alertas de incendio.\n\n'
                        'Comandos útiles:\n'
                        '• `/agregar` — Invitar a alguien más\n'
                        '• `/estado` — Ver estado del sistema\n'
                        '• `/usuarios` — Ver quién está vinculado'
                    )
                    print(f"✅ Padre vinculado: chat_id={chat_id}")

                # ── Código de hijo ──
                elif (config.get('owner_vinculado')
                      and not es_padre
                      and not any(h['chat_id'] == chat_id for h in config.get('hijos', []))):

                    pendientes = estado.get('pendientes_hijo', {})
                    codigo_up  = texto.upper()
                    match      = None
                    for cod, info in pendientes.items():
                        if cod == codigo_up and time.time() < info['expira']:
                            match = cod
                            break

                    if match:
                        config['hijos'].append({'chat_id': chat_id, 'nombre': nombre})
                        guardar_config(config)
                        del estado['pendientes_hijo'][match]

                        enviar_telegram(chat_id,
                            '✅ *Vinculación exitosa.*\n\nAhora recibirás alertas de incendio de FireGuard.'
                        )
                        enviar_telegram(config['chat_id'],
                            f'👤 *{nombre}* se ha unido como hijo a FireGuard.'
                        )
                        print(f"✅ Hijo vinculado: {nombre} ({chat_id})")
                    else:
                        enviar_telegram(chat_id,
                            '❌ Código incorrecto o expirado. Pide al padre que genere uno nuevo con `/agregar`.'
                        )

                # ── Eliminar hijo por chat_id ──
                elif estado.get(f'esperando_eliminar_{chat_id}') and es_padre:
                    target_id = texto.strip()
                    antes     = len(config.get('hijos', []))
                    config['hijos'] = [h for h in config.get('hijos', []) if h['chat_id'] != target_id]
                    if len(config['hijos']) < antes:
                        guardar_config(config)
                        del estado[f'esperando_eliminar_{chat_id}']
                        enviar_telegram(chat_id, f'✅ Usuario `{target_id}` eliminado.')
                        enviar_telegram(target_id, '❌ Has sido eliminado de FireGuard.')
                    else:
                        enviar_telegram(chat_id, f'⚠️ No encontré ese ID. Intenta de nuevo o manda /cancelar.')

        except Exception as e:
            print(f"⚠️  Error en bot loop: {e}")
            time.sleep(5)

        time.sleep(1)

# ─── Arranque ─────────────────────────────────────────────────────

if __name__ == '__main__':
    if not CONFIG_FILE.exists():
        guardar_config({"wifi_ssid": "", "chat_id": "", "owner_vinculado": False, "hijos": []})

    hilo_bot = threading.Thread(target=bot_loop, daemon=True)
    hilo_bot.start()

    print("🌐 Servidor iniciado en http://0.0.0.0:80")
    app.run(host='0.0.0.0', port=80, debug=False)
