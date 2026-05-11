"""
Aegnis - Controlador de LED RGB
LED RGB de anodo comun (HIGH = encendido, LOW = apagado)

Pines GPIO:
  Rojo  → GPIO 17 (pin 11)
  Verde → GPIO 27 (pin 13)
  Azul  → GPIO 22 (pin 15)

Estados:
  Azul parpadeando  → iniciando
  Verde fijo        → sistema activo, internet OK, camara OK
  Rojo parpadeando  → internet OK pero camara desconectada
  Rojo fijo         → sin internet
  Apagado           → error critico
"""

import time
import subprocess
import threading
from pathlib import Path

try:
    import RPi.GPIO as GPIO
    GPIO_DISPONIBLE = True
except ImportError:
    print("⚠️  RPi.GPIO no disponible. Corriendo en modo simulacion.")
    GPIO_DISPONIBLE = False

# ─── Pines GPIO ───────────────────────────────────────────
PIN_R = 17
PIN_G = 27
PIN_B = 22

CONFIG_FILE  = Path("/home/fisica1/sistema_incendios/config.json")
CAMARA_FLAG  = Path("/tmp/aegnis_camara_ok")
INTERVALO    = 5

# ─── Setup GPIO ───────────────────────────────────────────
if GPIO_DISPONIBLE:
    GPIO.setmode(GPIO.BCM)
    GPIO.setwarnings(False)
    GPIO.setup(PIN_R, GPIO.OUT)
    GPIO.setup(PIN_G, GPIO.OUT)
    GPIO.setup(PIN_B, GPIO.OUT)

# ─── Control del LED ──────────────────────────────────────
def set_led(r, g, b):
    """r, g, b = True/False (True = encendido)"""
    if not GPIO_DISPONIBLE:
        estado = []
        if r: estado.append('R')
        if g: estado.append('G')
        if b: estado.append('B')
        print(f"💡 LED: {'+'.join(estado) if estado else 'apagado'}")
        return
    GPIO.output(PIN_R, GPIO.HIGH if r else GPIO.LOW)
    GPIO.output(PIN_G, GPIO.HIGH if g else GPIO.LOW)
    GPIO.output(PIN_B, GPIO.HIGH if b else GPIO.LOW)

def apagar():
    set_led(False, False, False)

def verde():
    set_led(False, True, False)

def rojo():
    set_led(True, False, False)

def azul():
    set_led(False, False, True)

# ─── Parpadeo generico ────────────────────────────────────
parpadeo_activo = False
parpadeo_hilo   = None
parpadeo_lock   = threading.Lock()

def iniciar_parpadeo(color_fn, intervalo=0.5):
    global parpadeo_activo, parpadeo_hilo
    with parpadeo_lock:
        parpadeo_activo = True
        def _parpadear():
            while parpadeo_activo:
                color_fn()
                time.sleep(intervalo)
                apagar()
                time.sleep(intervalo)
        parpadeo_hilo = threading.Thread(target=_parpadear, daemon=True)
        parpadeo_hilo.start()

def detener_parpadeo():
    global parpadeo_activo
    with parpadeo_lock:
        parpadeo_activo = False
    time.sleep(1.1)
    apagar()

# ─── Chequeos de estado ───────────────────────────────────
def hay_internet():
    try:
        result = subprocess.run(
            ['ping', '-c', '1', '-W', '3', '8.8.8.8'],
            capture_output=True, timeout=5
        )
        return result.returncode == 0
    except Exception:
        return False

def hay_camara():
    try:
        if CAMARA_FLAG.exists():
            edad = time.time() - CAMARA_FLAG.stat().st_mtime
            return edad < 30
    except Exception:
        pass
    return False

# ─── Loop principal ───────────────────────────────────────
def loop():
    print("💡 Aegnis LED controller iniciando...")

    # Parpadeo azul durante el arranque (10 segundos)
    iniciar_parpadeo(azul, intervalo=0.5)
    time.sleep(10)
    detener_parpadeo()

    print("💡 Aegnis LED controller activo.")

    estado_anterior = None
    fallos_internet = 0

    while True:
        try:
            internet_ok = hay_internet()
            camara      = hay_camara()

            if internet_ok:
                fallos_internet = 0
                internet = True
            else:
                fallos_internet += 1
                internet = fallos_internet >= 3  # rojo solo tras 3 fallos seguidos
                internet = not (fallos_internet >= 3)

            if internet and camara:
                estado = "verde"
            elif internet and not camara:
                estado = "rojo_parpadeo"
            else:
                estado = "rojo"

            if estado != estado_anterior:
                detener_parpadeo()
                if estado == "verde":
                    verde()
                    print("💡 Estado: verde (todo OK)")
                elif estado == "rojo_parpadeo":
                    iniciar_parpadeo(rojo, intervalo=0.5)
                    print("💡 Estado: rojo parpadeando (camara desconectada)")
                elif estado == "rojo":
                    rojo()
                    print("💡 Estado: rojo fijo (sin internet)")
                estado_anterior = estado

        except Exception as e:
            print(f"⚠️  Error en LED loop: {e}")
            rojo()

        time.sleep(INTERVALO)

try:
    loop()
except KeyboardInterrupt:
    print("\n🛑 LED controller detenido.")
finally:
    detener_parpadeo()
    apagar()
    if GPIO_DISPONIBLE:
        GPIO.cleanup()
