// ============================================================
// PROYECTO INTEGRADOR 2 — Controlador de Brillo Adaptativo
// LED RGB cátodo común | Wokwi
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── PINES
#define PIN_LDR   A0
#define PIN_POT   A1
#define PIN_LED   3      // PWM
#define BTN_A     4      // cambia modo AUTO/MANUAL
#define BTN_B     5      // reset brillo a 50%

// ── MODOS
#define MODO_AUTO   0
#define MODO_MANUAL 1

// ── VARIABLES
int  modo          = MODO_AUTO;
int  brilloPWM     = 127;   // 0-255
int  brilloPct     = 50;    // 0-100 para mostrar
int  ldrVal        = 0;
int  ldrPrev       = 0;     // para histéresis REQ-07
float voltaje      = 0.0;

unsigned long tSerial  = 0;
unsigned long tDebA    = 0;
unsigned long tDebB    = 0;
unsigned long tOLED    = 0;

// ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  pinMode(PIN_LED, OUTPUT);
  pinMode(BTN_A,   INPUT_PULLUP);
  pinMode(BTN_B,   INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Error OLED"); while(true);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Iniciando...");
  display.display();
  delay(800);
}

// ────────────────────────────────────────────────────────────
void loop() {
  unsigned long ahora = millis();
  leerBotones(ahora);
  leerSensores(ahora);
  aplicarBrillo();
  enviarSerial(ahora);
  actualizarOLED(ahora);
}

// ── BOTONES
void leerBotones(unsigned long ahora) {
  // Botón A: alterna modo
  if (digitalRead(BTN_A) == LOW && ahora - tDebA > 200) {
    modo = (modo == MODO_AUTO) ? MODO_MANUAL : MODO_AUTO;
    tDebA = ahora;
  }
  // Botón B: reset brillo a 50%
  if (digitalRead(BTN_B) == LOW && ahora - tDebB > 200) {
    brilloPWM = 127;
    tDebB = ahora;
  }
}

// ── LECTURA SENSORES cada 200 ms (REQ-01)
void leerSensores(unsigned long ahora) {
  if (ahora - tOLED < 200) return;
  tOLED = ahora;

  // Leer LDR
  int lectura = analogRead(PIN_LDR);
  voltaje = lectura * (5.0 / 1023.0);

  // REQ-07: histéresis ±50 counts para evitar parpadeo
  if (abs(lectura - ldrPrev) >= 50) {
    ldrVal   = lectura;
    ldrPrev  = lectura;
  }

  if (modo == MODO_AUTO) {
    // REQ-02: inversamente proporcional — más luz = menos brillo
    brilloPWM = map(ldrVal, 0, 1023, 255, 0);
  } else {
    // REQ-03: modo manual — potenciómetro controla brillo
    int pot = analogRead(PIN_POT);
    brilloPWM = map(pot, 0, 1023, 0, 255);
  }

  brilloPct = map(brilloPWM, 0, 255, 0, 100);
}

// ── APLICAR PWM AL LED
void aplicarBrillo() {
  analogWrite(PIN_LED, brilloPWM);
}

// ── SERIAL cada 500 ms (REQ-06)
void enviarSerial(unsigned long ahora) {
  if (ahora - tSerial < 500) return;
  tSerial = ahora;

  String modoStr = (modo == MODO_AUTO) ? "AUTO" : "MANUAL";
  Serial.print("["); Serial.print(modoStr); Serial.print("] ");
  Serial.print("LDR: "); Serial.print(ldrVal);
  Serial.print(" | V: "); Serial.print(voltaje, 2);
  Serial.print("v | Brillo: "); Serial.print(brilloPct);
  Serial.println("%");
}

// ── OLED (REQ-05)
void actualizarOLED(unsigned long ahora) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Título / modo
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Modo: ");
  display.println(modo == MODO_AUTO ? "AUTOMATICO" : "MANUAL");
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  // LDR y voltaje
  display.setCursor(0, 13);
  display.print("ADC LDR: ");
  display.println(ldrVal);

  display.setCursor(0, 23);
  display.print("Voltaje: ");
  display.print(voltaje, 2);
  display.println(" V");

  // Brillo en número
  display.setCursor(0, 33);
  display.print("Brillo:  ");
  display.print(brilloPct);
  display.println(" %");

  // Barra gráfica de brillo (REQ-05)
  display.setCursor(0, 43);
  display.print("0%");
  display.setCursor(110, 43);
  display.print("100%");
  display.drawRect(0, 52, 128, 10, SSD1306_WHITE);
  int barraAncho = map(brilloPct, 0, 100, 0, 126);
  display.fillRect(1, 53, barraAncho, 8, SSD1306_WHITE);

  display.display();
}