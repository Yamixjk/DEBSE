// ============================================================
//  PROYECTO INTEGRADOR 5
//  Robot Seguidor de Línea con Sensores de Reflexión
//  y Control PWM
//
//  Plataforma  : Arduino UNO — Hardware Real
//  Sensores    : TCRT5000 x2 (salida analógica)
//  Motores     : 2x DC con driver L298N
//  Interfaz    : Pulsador inicio/stop + LED estado + Buzzer
//
//  Integrantes (orden alfabético por apellido):
//    1. Castro Márquez      Cristian
//    2. García Segura       América Yanely
//    3. Hernández Ruíz      Haydee Michelle
//    4. Montoya Garza       Luis Ángel
//    5. Navarro García      Maribel
//    6. Rueda Martínez      Alison Michelle
//    7. Silva Sánchez       Yamilka Arely
// ============================================================

// ── LIBRERÍAS ─────────────────────────────────────────────
// *****  #include <Servo.h>   // Solo para simulación en Wokwi

// ── PINES L298N ───────────────────────────────────────────
#define ENA  3   // PWM — habilita y controla velocidad motor izquierdo
#define IN1  7   // Dirección motor izquierdo (adelante/atrás)
#define IN2  8   // Dirección motor izquierdo (adelante/atrás)
#define ENB  5   // PWM — habilita y controla velocidad motor derecho
#define IN3 12   // Dirección motor derecho (adelante/atrás)
#define IN4 13   // Dirección motor derecho (adelante/atrás)

// ── PINES SENSORES IR ─────────────────────────────────────
#define PIN_SENSOR_IZQ A0  // Salida analógica sensor IR izquierdo
#define PIN_SENSOR_DER A1  // Salida analógica sensor IR derecho

// ── PINES INTERFAZ ────────────────────────────────────────
#define PIN_BOTON  2   // Pulsador inicio/stop (VCC → D2, pull-down 10k a GND)
#define PIN_LED    4   // LED verde — indica si el robot está activo
#define PIN_BUZZER 6   // Buzzer pasivo PWM — tonos de arranque y parada

// ── CALIBRACIÓN DE SENSORES ───────────────────────────────
// El TCRT5000 devuelve valores altos (~970) sobre negro
// y valores bajos (~70) sobre blanco.
// El umbral de 500 queda justo en el punto medio.
#define UMBRAL_LINEA 500

// ── VELOCIDADES (0–255) ───────────────────────────────────
#define VEL_BASE  140  // Velocidad de avance recto — moderada para leer sensores
#define VEL_GIRO   42  // ~30% de VEL_BASE — motor interior en curvas
#define VEL_BUSQ  100  // Velocidad de giro durante rutina de búsqueda de línea

// ── TIEMPOS (millis) ──────────────────────────────────────
#define INTERVALO_LECTURA   20UL   // Lectura de sensores cada 20 ms (50 Hz) — REQ-01
#define INTERVALO_SERIAL   100UL   // Impresión Serial cada 100 ms — REQ-07
#define TIEMPO_PERDIDO     500UL   // Sin línea por 500 ms activa rutina de búsqueda — REQ-04

// ── ESTADOS DE LA MÁQUINA ─────────────────────────────────
// REQ-02: define el comportamiento del robot según lo que ven los sensores
enum Estado { RECTO, GIRO_IZQ, GIRO_DER, BUSQUEDA };
Estado estadoActual = RECTO;

// ── VARIABLES DE CONTROL ──────────────────────────────────
bool robotActivo  = false;  // true = robot en marcha, false = detenido
bool btnAnterior  = false;  // Estado anterior del botón para detectar flanco

int valIzq       = 0;  // Valor ADC del sensor izquierdo (0–1023)
int valDer       = 0;  // Valor ADC del sensor derecho   (0–1023)
int velIzqActual = 0;  // Velocidad actual motor izquierdo (para Serial)
int velDerActual = 0;  // Velocidad actual motor derecho   (para Serial)

unsigned long tLectura = 0;  // Timestamp última lectura de sensores
unsigned long tSerial  = 0;  // Timestamp último envío Serial
unsigned long tPerdido = 0;  // Timestamp cuando ambos sensores perdieron la línea
bool perdidoActivo     = false;  // true = está contando el tiempo sin línea

// ── VARIABLES BUZZER NO BLOQUEANTE ────────────────────────
// Máquina de estados para los tonos del buzzer sin usar delay()
// Secuencia arranque : 1 tono 1000 Hz × 200 ms
// Secuencia parada   : 800 Hz × 100 ms → silencio 150 ms → 600 Hz × 150 ms
enum EstadoBuzzer { BUZ_IDLE, BUZ_ARRANQUE_1, BUZ_STOP_1, BUZ_STOP_PAUSA, BUZ_STOP_2 };
EstadoBuzzer estadoBuzzer = BUZ_IDLE;
unsigned long tBuzzer     = 0;


// ============================================================
//  BUZZER — Inicia secuencia de arranque
//  Llamar al activar el robot. No bloquea el loop.
// ============================================================
void iniciarBuzzerArranque() {
  tone(PIN_BUZZER, 1000);
  tBuzzer      = millis();
  estadoBuzzer = BUZ_ARRANQUE_1;
}


// ============================================================
//  BUZZER — Inicia secuencia de parada
//  Llamar al detener el robot. No bloquea el loop.
// ============================================================
void iniciarBuzzerStop() {
  tone(PIN_BUZZER, 800);
  tBuzzer      = millis();
  estadoBuzzer = BUZ_STOP_1;
}


// ============================================================
//  actualizarBuzzer — Avanza la máquina de estados del buzzer
//
//  Se llama en cada iteración del loop().
//  Controla la duración de cada tono y las pausas entre ellos
//  sin usar delay(), manteniendo el loop libre para otras tareas.
//
//  Estados:
//    BUZ_IDLE        → sin actividad, no hace nada
//    BUZ_ARRANQUE_1  → espera 200 ms y apaga tono de arranque
//    BUZ_STOP_1      → espera 100 ms, apaga primer tono de parada
//    BUZ_STOP_PAUSA  → espera 150 ms en silencio
//    BUZ_STOP_2      → espera 150 ms y apaga segundo tono de parada
// ============================================================
void actualizarBuzzer() {
  if (estadoBuzzer == BUZ_IDLE) return;
  unsigned long ahora = millis();

  switch (estadoBuzzer) {
    case BUZ_ARRANQUE_1:
      if (ahora - tBuzzer >= 200) {
        noTone(PIN_BUZZER);
        estadoBuzzer = BUZ_IDLE;
      }
      break;

    case BUZ_STOP_1:
      if (ahora - tBuzzer >= 100) {
        noTone(PIN_BUZZER);
        tBuzzer      = ahora;
        estadoBuzzer = BUZ_STOP_PAUSA;
      }
      break;

    case BUZ_STOP_PAUSA:
      if (ahora - tBuzzer >= 150) {
        tone(PIN_BUZZER, 600);
        tBuzzer      = ahora;
        estadoBuzzer = BUZ_STOP_2;
      }
      break;

    case BUZ_STOP_2:
      if (ahora - tBuzzer >= 150) {
        noTone(PIN_BUZZER);
        estadoBuzzer = BUZ_IDLE;
      }
      break;

    default: break;
  }
}


// ============================================================
//  motorIzq — Controla velocidad y dirección del motor izquierdo
//
//  Parámetros:
//    velocidad : 0–255 (0 = parado)
//    adelante  : true = avanza, false = retrocede
//
//  IN1/IN2 determinan la dirección según la tabla del L298N:
//    IN1=HIGH, IN2=LOW  → adelante
//    IN1=LOW,  IN2=HIGH → atrás
//  ENA controla la velocidad mediante PWM (analogWrite).
// ============================================================
void motorIzq(int velocidad, bool adelante) {
  // ++++
  digitalWrite(IN1, adelante ? HIGH : LOW);
  digitalWrite(IN2, adelante ? LOW  : HIGH);
  analogWrite(ENA, velocidad);
  velIzqActual = velocidad;
  // ++++
}


// ============================================================
//  motorDer — Controla velocidad y dirección del motor derecho
//
//  Igual que motorIzq pero para el canal B del L298N.
//  IN3/IN4 controlan dirección, ENB controla velocidad.
// ============================================================
void motorDer(int velocidad, bool adelante) {
  // ++++
  digitalWrite(IN3, adelante ? HIGH : LOW);
  digitalWrite(IN4, adelante ? LOW  : HIGH);
  analogWrite(ENB, velocidad);
  velDerActual = velocidad;
  // ++++
}


// ============================================================
//  detenerMotores — Detiene ambos motores inmediatamente
//  Llama a motorIzq y motorDer con velocidad 0.
// ============================================================
void detenerMotores() {
  motorIzq(0, true);
  motorDer(0, true);
  velIzqActual = 0;
  velDerActual = 0;
}


// ============================================================
//  leerBoton — Detecta pulsación del botón inicio/stop
//
//  Conexión: VCC → botón → D2, con resistencia 10k a GND.
//  Reposo = LOW, presionado = HIGH.
//  Se detecta flanco de subida (LOW→HIGH) para alternar el
//  estado del robot sin repetir la acción mientras se mantiene.
//
//  Al activar  : enciende LED, inicia buzzer de arranque.
//  Al desactivar: apaga LED, detiene motores, inicia buzzer de parada.
// ============================================================
void leerBoton() {
  bool btnActual = (digitalRead(PIN_BOTON) == HIGH);

  if (btnActual && !btnAnterior) {
    robotActivo = !robotActivo;

    if (robotActivo) {
      digitalWrite(PIN_LED, HIGH);
      iniciarBuzzerArranque();
      Serial.println(">>> ROBOT ACTIVO");
    } else {
      digitalWrite(PIN_LED, LOW);
      detenerMotores();
      iniciarBuzzerStop();
      estadoActual  = RECTO;
      perdidoActivo = false;
      Serial.println(">>> ROBOT DETENIDO");
    }
  }

  btnAnterior = btnActual;
}


// ============================================================
//  actualizarEstado — Máquina de estados del seguidor de línea
//
//  Lee los valores ADC de ambos sensores y decide el movimiento.
//  Se ejecuta cada 20 ms (50 Hz) para cumplir REQ-01.
//
//  Lógica de seguimiento (REQ-02):
//    Izq=negro, Der=negro → RECTO      : ambos motores a VEL_BASE
//    Izq=blanco,Der=negro → GIRO_IZQ  : motor izq lento, der rápido
//    Izq=negro, Der=blanco→ GIRO_DER  : motor izq rápido, der lento
//    Izq=blanco,Der=blanco→ sin línea : espera TIEMPO_PERDIDO ms
//                                       luego activa BUSQUEDA
//
//  Control de velocidad (REQ-03):
//    El motor interior en curva recibe VEL_GIRO (~30% de VEL_BASE)
//    para forzar el giro sin detener el avance del robot.
//
//  Rutina de búsqueda (REQ-04):
//    Si ambos sensores ven blanco por más de 500 ms, el robot
//    gira sobre sí mismo (izq adelante, der atrás) buscando la línea.
// ============================================================
void actualizarEstado() {
  bool lineaIzq = (valIzq > UMBRAL_LINEA);
  bool lineaDer = (valDer > UMBRAL_LINEA);

  if (lineaIzq && lineaDer) {
    estadoActual  = RECTO;
    perdidoActivo = false;
    motorIzq(VEL_BASE, true);
    motorDer(VEL_BASE, true);

  } else if (!lineaIzq && lineaDer) {
    estadoActual  = GIRO_IZQ;
    perdidoActivo = false;
    motorIzq(VEL_GIRO, true);
    motorDer(VEL_BASE, true);

  } else if (lineaIzq && !lineaDer) {
    estadoActual  = GIRO_DER;
    perdidoActivo = false;
    motorIzq(VEL_BASE, true);
    motorDer(VEL_GIRO, true);

  } else {
    // Ningún sensor sobre línea: iniciar contador de tiempo perdido
    if (!perdidoActivo) {
      perdidoActivo = true;
      tPerdido      = millis();
    }
    // Después de 500 ms sin línea, activar rutina de búsqueda
    if (millis() - tPerdido >= TIEMPO_PERDIDO) {
      estadoActual = BUSQUEDA;
      motorIzq(VEL_BUSQ, true);
      motorDer(VEL_BUSQ, false);
    }
  }
}


// ============================================================
//  nombreEstado — Retorna el nombre textual del estado actual
//
//  Usado en imprimirSerial() para el formato REQ-07.
//  Retorna:
//    "RECTO"    → avanzando en línea recta
//    "GIRO_IZQ" → corrigiendo hacia la izquierda
//    "GIRO_DER" → corrigiendo hacia la derecha
//    "BUSQUEDA" → buscando la línea tras perderla
// ============================================================
const char* nombreEstado(Estado e) {
  switch (e) {
    case RECTO:    return "RECTO";
    case GIRO_IZQ: return "GIRO_IZQ";
    case GIRO_DER: return "GIRO_DER";
    case BUSQUEDA: return "BUSQUEDA";
    default:       return "STOP";
  }
}


// ============================================================
//  imprimirSerial — Envía datos de estado por Monitor Serial
//
//  Formato (REQ-07), cada 100 ms:
//  [ESTADO] Sen_izq: X | Sen_der: X | Vel_izq: X | Vel_der: X
//
//  Campos:
//    ESTADO   → estado actual de la máquina o "STOP" si inactivo
//    Sen_izq  → valor ADC sensor izquierdo (0–1023)
//    Sen_der  → valor ADC sensor derecho   (0–1023)
//    Vel_izq  → velocidad PWM motor izquierdo (0–255)
//    Vel_der  → velocidad PWM motor derecho   (0–255)
// ============================================================
void imprimirSerial() {
  Serial.print("[");
  Serial.print(robotActivo ? nombreEstado(estadoActual) : "STOP");
  Serial.print("] Sen_izq: ");
  Serial.print(valIzq);
  Serial.print(" | Sen_der: ");
  Serial.print(valDer);
  Serial.print(" | Vel_izq: ");
  Serial.print(velIzqActual);
  Serial.print(" | Vel_der: ");
  Serial.println(velDerActual);
}


// ============================================================
//  SETUP — Se ejecuta una sola vez al encender o resetear
// ============================================================
void setup() {
  Serial.begin(9600);

  // Pines de dirección y habilitación del L298N
  // ++++
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT); pinMode(ENB, OUTPUT);
  // ++++

  // Pines de interfaz
  pinMode(PIN_BOTON,  INPUT);
  pinMode(PIN_LED,    OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  // Asegurar que los motores arrancan detenidos
  detenerMotores();

  Serial.println("Sistema listo. Presiona el boton para arrancar.");
}


// ============================================================
//  LOOP — Se ejecuta continuamente
//  Usa millis() para coordinar tareas sin bloquear el programa
// ============================================================
void loop() {
  unsigned long ahora = millis();

  leerBoton();        // Siempre: detectar pulsación del botón
  actualizarBuzzer(); // Siempre: avanzar máquina de estados del buzzer

  // Cada 20 ms: leer sensores y actualizar estado de movimiento
  if (ahora - tLectura >= INTERVALO_LECTURA) {
    tLectura = ahora;
    valIzq = analogRead(PIN_SENSOR_IZQ);
    valDer = analogRead(PIN_SENSOR_DER);
    if (robotActivo) actualizarEstado();
  }

  // Cada 100 ms: enviar datos al Monitor Serial
  if (ahora - tSerial >= INTERVALO_SERIAL) {
    tSerial = ahora;
    imprimirSerial();
  }
}
