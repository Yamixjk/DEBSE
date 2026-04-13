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
#define TIEMPO_PERDIDO     500UL   // Sin línea por 500 ms activa rutina de búsqueda — REQ-04

// ── ESTADOS DE LA MÁQUINA ─────────────────────────────────
enum Estado { RECTO, GIRO_IZQ, GIRO_DER, BUSQUEDA };
Estado estadoActual = RECTO;

// ── VARIABLES DE CONTROL ──────────────────────────────────
bool robotActivo  = false;
bool btnAnterior  = false;

int valIzq = 0;
int valDer = 0;

unsigned long tLectura = 0;
unsigned long tPerdido = 0;
bool perdidoActivo     = false;

// ── VARIABLES BUZZER NO BLOQUEANTE ────────────────────────
enum EstadoBuzzer { BUZ_IDLE, BUZ_ARRANQUE_1, BUZ_STOP_1, BUZ_STOP_PAUSA, BUZ_STOP_2 };
EstadoBuzzer estadoBuzzer = BUZ_IDLE;
unsigned long tBuzzer     = 0;


// ============================================================
//  BUZZER — Inicia secuencia de arranque
// ============================================================
void iniciarBuzzerArranque() {
  tone(PIN_BUZZER, 1000);
  tBuzzer      = millis();
  estadoBuzzer = BUZ_ARRANQUE_1;
}


// ============================================================
//  BUZZER — Inicia secuencia de parada
// ============================================================
void iniciarBuzzerStop() {
  tone(PIN_BUZZER, 800);
  tBuzzer      = millis();
  estadoBuzzer = BUZ_STOP_1;
}


// ============================================================
//  actualizarBuzzer — Avanza la máquina de estados del buzzer
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
// ============================================================
void motorIzq(int velocidad, bool adelante) {
  digitalWrite(IN1, adelante ? HIGH : LOW);
  digitalWrite(IN2, adelante ? LOW  : HIGH);
  analogWrite(ENA, velocidad);
}


// ============================================================
//  motorDer — Controla velocidad y dirección del motor derecho
// ============================================================
void motorDer(int velocidad, bool adelante) {
  digitalWrite(IN3, adelante ? HIGH : LOW);
  digitalWrite(IN4, adelante ? LOW  : HIGH);
  analogWrite(ENB, velocidad);
}


// ============================================================
//  detenerMotores — Detiene ambos motores inmediatamente
// ============================================================
void detenerMotores() {
  motorIzq(0, true);
  motorDer(0, true);
}


// ============================================================
//  leerBoton — Detecta pulsación del botón inicio/stop
// ============================================================
void leerBoton() {
  bool btnActual = (digitalRead(PIN_BOTON) == HIGH);

  if (btnActual && !btnAnterior) {
    robotActivo = !robotActivo;

    if (robotActivo) {
      digitalWrite(PIN_LED, HIGH);
      iniciarBuzzerArranque();
    } else {
      digitalWrite(PIN_LED, LOW);
      detenerMotores();
      iniciarBuzzerStop();
      estadoActual  = RECTO;
      perdidoActivo = false;
    }
  }

  btnAnterior = btnActual;
}


// ============================================================
//  actualizarEstado — Máquina de estados del seguidor de línea
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
    if (!perdidoActivo) {
      perdidoActivo = true;
      tPerdido      = millis();
    }
    if (millis() - tPerdido >= TIEMPO_PERDIDO) {
      estadoActual = BUSQUEDA;
      motorIzq(VEL_BUSQ, true);
      motorDer(VEL_BUSQ, false);
    }
  }
}


// ============================================================
//  SETUP — Se ejecuta una sola vez al encender o resetear
// ============================================================
void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  pinMode(PIN_BOTON,  INPUT);
  pinMode(PIN_LED,    OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  detenerMotores();
}


// ============================================================
//  LOOP — Se ejecuta continuamente
// ============================================================
void loop() {
  unsigned long ahora = millis();

  leerBoton();
  actualizarBuzzer();

  if (ahora - tLectura >= INTERVALO_LECTURA) {
    tLectura = ahora;
    valIzq = analogRead(PIN_SENSOR_IZQ);
    valDer = analogRead(PIN_SENSOR_DER);
    if (robotActivo) actualizarEstado();
  }
}
