import cv2
import json
import requests
import time
import threading
from pathlib import Path
from ultralytics import YOLO

# ─── CONFIGURACIÓN ────────────────────────────────────────
TOKEN            = "8520152162:AAEMYYcvky9gMfiw_BX-XMZ960pEFU0IpRw"
MODELO           = "/home/fisica1/sistema_incendios/best.onnx"
CONFIG_FILE      = Path("/home/fisica1/sistema_incendios/config.json")
CAMARA_FLAG      = Path("/tmp/aegnis_camara_ok")
FRAME_STREAM     = Path("/tmp/aegnis_frame.jpg")
CONFIANZA_MINIMA = 0.5
COOLDOWN_ALERTAS = 30    # segundos entre alertas de incendio
COOLDOWN_CAMARA  = 60    # segundos entre avisos de cámara caída
# ──────────────────────────────────────────────────────────

def cargar_chat_ids():
    """Lee todos los chat_ids (padre + hijos) del config."""
    try:
        if CONFIG_FILE.exists():
            with open(CONFIG_FILE) as f:
                config = json.load(f)
            ids = []
            if config.get('chat_id'):
                ids.append(config['chat_id'])
            for hijo in config.get('hijos', []):
                if hijo.get('chat_id'):
                    ids.append(hijo['chat_id'])
            return ids
    except Exception as e:
        print(f"⚠️  Error leyendo config: {e}")
    return []

def enviar_mensaje(chat_id, texto):
    try:
        requests.post(
            f"https://api.telegram.org/bot{TOKEN}/sendMessage",
            json={'chat_id': chat_id, 'text': texto, 'parse_mode': 'Markdown'},
            timeout=10
        )
    except Exception as e:
        print(f"❌ Error enviando mensaje: {e}")

def notificar_todos(texto):
    for cid in cargar_chat_ids():
        enviar_mensaje(cid, texto)

def enviar_foto_a_todos(imagen_path, caption):
    for cid in cargar_chat_ids():
        try:
            with open(imagen_path, 'rb') as img:
                requests.post(
                    f"https://api.telegram.org/bot{TOKEN}/sendPhoto",
                    data={'chat_id': cid, 'caption': caption, 'parse_mode': 'Markdown'},
                    files={'photo': img},
                    timeout=10
                )
        except Exception as e:
            print(f"❌ Error enviando foto a {cid}: {e}")

# ─── Inicializar modelo ───────────────────────────────────
print("🔥 FireGuard detector iniciando...")

try:
    model = YOLO(MODELO, task='detect')
    print("✅ Modelo cargado correctamente.")
except Exception as e:
    print(f"❌ Error cargando modelo: {e}")
    notificar_todos(f"❌ *FireGuard — Error crítico*\n\nNo se pudo cargar el modelo de detección.\n`{e}`")
    exit(1)

# ─── Inicializar cámara ───────────────────────────────────
def abrir_camara():
    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        return None
    return cap

cap = abrir_camara()
if cap is None:
    print("❌ No se encontró la cámara al iniciar.")
    notificar_todos("📷 *FireGuard — Sin cámara*\n\nNo se detectó la cámara al arrancar el sistema. Verifica la conexión.")
else:
    print("✅ Cámara inicializada.")
    notificar_todos("✅ *FireGuard activo*\n\nEl sistema de detección de incendios está en línea.")

# ─── Variables globales ───────────────────────────────────
ultima_alerta_incendio = 0
ultima_alerta_camara   = 0
detecciones_actuales   = []
procesando             = False

# ─── Funciones ────────────────────────────────────────────

def procesar_frame(frame):
    global detecciones_actuales, procesando
    procesando = True
    try:
        results = model(frame, conf=CONFIANZA_MINIMA, verbose=False)
        nuevas  = []
        for box in results[0].boxes:
            clase     = int(box.cls)
            confianza = float(box.conf)
            x1, y1, x2, y2 = map(int, box.xyxy[0])
            area_frame = frame.shape[0] * frame.shape[1]
            area_box   = (x2 - x1) * (y2 - y1)
            porcentaje = area_box / area_frame
            if clase == 1:
                nivel, descripcion = "ATENCION", "Humo"
            elif porcentaje < 0.02:
                nivel, descripcion = "ADVERTENCIA", "Llama pequeña"
            elif porcentaje < 0.08:
                nivel, descripcion = "PELIGRO MEDIO", "Fuego en desarrollo"
            else:
                nivel, descripcion = "PELIGRO ALTO", "Incendio"
            nuevas.append((clase, confianza, x1, y1, x2, y2, nivel, descripcion))
        detecciones_actuales = nuevas

        # Dibujar recuadros y guardar frame para el stream web
        colores = {"ADVERTENCIA": (0,255,255), "ATENCION": (0,165,255), "PELIGRO MEDIO": (0,100,255), "PELIGRO ALTO": (0,0,255)}
        frame_anotado = frame.copy()
        for (clase, conf, x1, y1, x2, y2, nivel, desc) in nuevas:
            color = colores.get(nivel, (0,255,0))
            cv2.rectangle(frame_anotado, (x1,y1), (x2,y2), color, 2)
            cv2.putText(frame_anotado, f"{desc} {conf:.0%}", (x1, y1-8),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)
        cv2.imwrite(str(FRAME_STREAM), frame_anotado)

    except Exception as e:
        print(f"⚠️  Error procesando frame: {e}")
    procesando = False

def enviar_alerta_incendio(imagen_path, detecciones):
    prioridad = {"ADVERTENCIA": 1, "ATENCION": 2, "PELIGRO MEDIO": 3, "PELIGRO ALTO": 4}
    nivel_max = max(detecciones, key=lambda x: prioridad.get(x[6], 0))
    nivel     = nivel_max[6]
    emojis    = {"ADVERTENCIA": "⚠️", "ATENCION": "👁️", "PELIGRO MEDIO": "🔥", "PELIGRO ALTO": "🚨"}
    texto = f"{emojis[nivel]} *{nivel}*\n\n"
    for det in detecciones:
        texto += f"• {det[7]} — {det[1]:.1%} confianza\n"
    enviar_foto_a_todos(imagen_path, texto)
    print(f"✅ Alerta enviada: {nivel}")

# ─── Loop principal ───────────────────────────────────────
print("🔥 FireGuard corriendo en modo headless (sin ventana de video).")
frame_count = 0

try:
    while True:
        # Si la cámara está caída, intentar reconectar
        if cap is None or not cap.isOpened():
            ahora = time.time()
            if ahora - ultima_alerta_camara > COOLDOWN_CAMARA:
                print("⚠️  Cámara desconectada. Intentando reconectar...")
                notificar_todos("📷 *FireGuard — Cámara desconectada*\n\nSe perdió la señal de la cámara. Intentando reconectar...")
                ultima_alerta_camara = ahora

            cap = abrir_camara()
            if cap is not None:
                print("✅ Cámara reconectada.")
                notificar_todos("📷 *FireGuard — Cámara reconectada*\n\nLa cámara volvió a estar disponible.")
            else:
                time.sleep(10)
                continue

        ret, frame = cap.read()
        if not ret:
            print("⚠️  Error leyendo frame.")
            cap.release()
            cap = None
            CAMARA_FLAG.unlink(missing_ok=True)
            continue

        # Actualizar flag de camara activa (lo lee el controlador de LED)
        CAMARA_FLAG.touch()

        frame_count += 1
        if frame_count % 3 == 0 and not procesando:
            hilo = threading.Thread(target=procesar_frame, args=(frame.copy(),))
            hilo.daemon = True
            hilo.start()

        ahora = time.time()
        if detecciones_actuales and (ahora - ultima_alerta_incendio) > COOLDOWN_ALERTAS:
            imagen_path = "/home/fisica1/sistema_incendios/alerta.jpg"
            cv2.imwrite(imagen_path, frame)
            hilo_alerta = threading.Thread(
                target=enviar_alerta_incendio,
                args=(imagen_path, detecciones_actuales[:])
            )
            hilo_alerta.daemon = True
            hilo_alerta.start()
            ultima_alerta_incendio = ahora

        time.sleep(0.05)

except KeyboardInterrupt:
    print("\n🛑 Sistema detenido manualmente.")
finally:
    if cap:
        cap.release()
    print("Cámara liberada.")
