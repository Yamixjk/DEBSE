// ============================================================
// PROYECTO INTEGRADOR 3 — Control de Acceso con Pulsadores
// Wokwi | Arduino UNO
// ============================================================

#include <Servo.h>

// ── PINES
#define BTN1  2
#define BTN2  3
#define BTN3  4
#define BTN4  5
#define LED_VERDE    6
#define LED_ROJO     7
#define LED_AMARILLO 8
#define SERVO_PIN    9
#define BUZZER       10

// ── CÓDIGO CORRECTO (modificable)
const int CODIGO[4] = {1, 3, 2, 4};

// ── ESTADOS
#define ESPERA           0
#define INGRESANDO       1
#define ABIERTO          2
#define BLOQUEADO_CORTO  3
#define BLOQUEADO_LARGO  4

// ── VARIABLES
int  estado          = ESPERA;
int  ingresado[4]    = {0,0,0,0};
int  posicion        = 0;   // cuántos dígitos lleva
int  fallos          = 0;   // intentos fallidos consecutivos
int  numIntento      = 0;   // contador global de intentos

unsigned long tEstado  = 0;  // temporizador del estado actual
unsigned long tDebBtn  = 0;
unsigned long tBuzzer  = 0;
unsigned long tBlink   = 0;  // parpadeo LED amarillo

bool buzzerON    = false;
int  buzzerPaso  = 0;
int  buzzerTotal = 0;
bool buzzerLargo = false;
bool ledAmarillo = false;

Servo servo;

// ── SETUP
void setup() {
  Serial.begin(9600);
  pinMode(BTN1,        INPUT_PULLUP);
  pinMode(BTN2,        INPUT_PULLUP);
  pinMode(BTN3,        INPUT_PULLUP);
  pinMode(BTN4,        INPUT_PULLUP);
  pinMode(LED_VERDE,   OUTPUT);
  pinMode(LED_ROJO,    OUTPUT);
  pinMode(LED_AMARILLO,OUTPUT);
  pinMode(BUZZER,      OUTPUT);

  servo.attach(SERVO_PIN);
  servo.write(0);  // cerrado

  apagarLEDs();
  digitalWrite(LED_VERDE, HIGH);  // verde al inicio = listo
  delay(500);
  apagarLEDs();
}

// ── LOOP
void loop() {
  unsigned long ahora = millis();
  leerBotones(ahora);
  actualizarEstado(ahora);
  gestionarBuzzer(ahora);
  parpadeoAmarillo(ahora);
}

// ── APAGAR TODOS LOS LEDs
void apagarLEDs() {
  digitalWrite(LED_VERDE,    LOW);
  digitalWrite(LED_ROJO,     LOW);
  digitalWrite(LED_AMARILLO, LOW);
}

// ── LECTURA BOTONES
void leerBotones(unsigned long ahora) {
  if (estado == BLOQUEADO_CORTO || estado == BLOQUEADO_LARGO) return;
  if (ahora - tDebBtn < 200) return;

  int digito = 0;
  if (digitalRead(BTN1) == LOW) digito = 1;
  else if (digitalRead(BTN2) == LOW) digito = 2;
  else if (digitalRead(BTN3) == LOW) digito = 3;
  else if (digitalRead(BTN4) == LOW) digito = 4;

  if (digito == 0) return;
  tDebBtn = ahora;

  if (estado == ESPERA) {
    estado = INGRESANDO;
    posicion = 0;
  }

  if (estado == INGRESANDO) {
    ingresado[posicion] = digito;
    posicion++;

    // Parpadeo rápido del amarillo al presionar
    apagarLEDs();
    digitalWrite(LED_AMARILLO, HIGH);

    if (posicion == 4) {
      verificarCodigo(ahora);
    }
  }
}

// ── VERIFICAR CÓDIGO
void verificarCodigo(unsigned long ahora) {
  numIntento++;
  bool correcto = true;
  for (int i = 0; i < 4; i++) {
    if (ingresado[i] != CODIGO[i]) { correcto = false; break; }
  }

  // Imprimir en Serial REQ-07
  Serial.print("[intento#"); Serial.print(numIntento); Serial.print("] ");
  Serial.print("Ingresado: ");
  for (int i = 0; i < 4; i++) Serial.print(ingresado[i]);
  Serial.print(" | Resultado: ");
  Serial.print(correcto ? "OK" : "FAIL");
  Serial.print(" | Intentos_fallidos: ");
  Serial.println(correcto ? 0 : fallos + 1);

  if (correcto) {
    fallos = 0;
    estado = ABIERTO;
    tEstado = ahora;
    apagarLEDs();
    digitalWrite(LED_VERDE, HIGH);
    servo.write(90);
    iniciarBuzzer(2, false);  // 2 tonos ascendentes
  } else {
    fallos++;
    if (fallos >= 3) {
      estado = BLOQUEADO_LARGO;
    } else {
      estado = BLOQUEADO_CORTO;
    }
    tEstado = ahora;
    apagarLEDs();
    digitalWrite(LED_ROJO, HIGH);
    iniciarBuzzer(4, false);  // tono grave repetitivo
  }
}

// ── MÁQUINA DE ESTADOS (temporizadores)
void actualizarEstado(unsigned long ahora) {
  switch (estado) {
    case ABIERTO:
      // Cierre automático a los 5 segundos
      if (ahora - tEstado >= 5000) {
        servo.write(0);
        apagarLEDs();
        estado = ESPERA;
        posicion = 0;
      }
      break;

    case BLOQUEADO_CORTO:
      // Bloqueo 5 segundos
      if (ahora - tEstado >= 5000) {
        apagarLEDs();
        estado = ESPERA;
        posicion = 0;
      }
      break;

    case BLOQUEADO_LARGO:
      // Bloqueo 30 segundos
      if (ahora - tEstado >= 30000) {
        fallos = 0;
        apagarLEDs();
        estado = ESPERA;
        posicion = 0;
      }
      break;

    case INGRESANDO:
      apagarLEDs();
      // El parpadeoAmarillo() se encarga del LED
      break;

    default:
      break;
  }
}

// ── PARPADEO LED AMARILLO mientras ingresa
void parpadeoAmarillo(unsigned long ahora) {
  if (estado != INGRESANDO) return;
  if (ahora - tBlink < 300) return;
  tBlink = ahora;
  ledAmarillo = !ledAmarillo;
  digitalWrite(LED_AMARILLO, ledAmarillo);
}

// ── BUZZER NO BLOQUEANTE
void iniciarBuzzer(int pulsos, bool largo) {
  buzzerTotal = pulsos * 2;
  buzzerPaso  = 0;
  buzzerLargo = largo;
  buzzerON    = true;
  tBuzzer     = millis() - 9999;
}

void gestionarBuzzer(unsigned long ahora) {
  if (!buzzerON) return;
  unsigned long durON  = buzzerLargo ? 600 : 150;
  unsigned long durOFF = buzzerLargo ? 150 : 100;
  unsigned long dur    = (buzzerPaso % 2 == 0) ? durON : durOFF;

  if (ahora - tBuzzer >= dur) {
    tBuzzer = ahora;
    if (buzzerPaso % 2 == 0) {
      // Tono ascendente en éxito, grave en fallo
      int freq = (estado == ABIERTO) ? (800 + buzzerPaso * 200) : 300;
      tone(BUZZER, freq);
    } else {
      noTone(BUZZER);
    }
    buzzerPaso++;
    if (buzzerPaso >= buzzerTotal) {
      noTone(BUZZER);
      buzzerON = false;
    }
  }
}