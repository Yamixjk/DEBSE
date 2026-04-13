// ============================================================
// PROYECTO INTEGRADOR 1 — Sistema de Alerta Temp & Humedad
// LED RGB cátodo común
// ============================================================

//Librerias
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ── Inicializar la OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── inicializar el DHT22 con la libreria y definir los pines
#define DHTPIN  2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ── PINES
//LED RGB
#define RGB_R    9
#define RGB_G    10
#define RGB_B    11
//BUZZER
#define BUZZER   8
//BOTONES
#define BTN_SUBE 4
#define BTN_BAJA 5

// ── ESTADOS
#define NORMAL      0
#define ADVERTENCIA 1
#define ALARMA      2

// ── VARIABLES
float temperatura = 0.0;
float humedad     = 0.0;
int   umbralTemp  = 30;
int   estado      = NORMAL;
int   estadoPrev  = -1;

unsigned long tLectura  = 0;
unsigned long tBuzzer   = 0;
unsigned long tArranque = 0;
unsigned long tDebBtn   = 0;

bool buzzerON    = false;
int  buzzerPaso  = 0;
int  buzzerTotal = 0;
bool buzzerLargo = false;

// ── RGB helper
void setRGB(bool r, bool g, bool b) {
  digitalWrite(RGB_R, r);
  digitalWrite(RGB_G, g);
  digitalWrite(RGB_B, b);
}

// ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  //definir pines del led como salida
  pinMode(RGB_R,    OUTPUT);
  pinMode(RGB_G,    OUTPUT);
  pinMode(RGB_B,    OUTPUT);
  pinMode(BUZZER,   OUTPUT);
  //botones como entrada con la resisitencia del arduino
  pinMode(BTN_SUBE, INPUT_PULLUP);
  pinMode(BTN_BAJA, INPUT_PULLUP);

  setRGB(0,0,0);
  dht.begin();
  // comprobar que funciona el oled
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Error OLED"); while(true);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Iniciando...");
  display.display();
  tArranque = millis();
  delay(1000);
}

// LOOP PRINCIPAL
void loop() {
  unsigned long ahora = millis();
  leerBotones(ahora);
  leerSensores(ahora);
  actualizarEstado();
  actualizarLED();
  gestionarBuzzer(ahora);
  actualizarOLED(ahora);
}

// ── BOTONES debounce 200 ms
void leerBotones(unsigned long ahora) {
  if (ahora - tDebBtn < 200) return;
  if (digitalRead(BTN_SUBE) == LOW) { if (umbralTemp < 40) umbralTemp++; tDebBtn = ahora; }
  if (digitalRead(BTN_BAJA) == LOW) { if (umbralTemp > 20) umbralTemp--; tDebBtn = ahora; }
}

// ── LECTURA cada 2 s
void leerSensores(unsigned long ahora) {
  if (ahora - tLectura < 2000) return;
  tLectura = ahora;
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperatura = t;
  if (!isnan(h)) humedad     = h;

  String est = (estado == NORMAL) ? "NORMAL" :
               (estado == ADVERTENCIA) ? "ADVERTENCIA" : "ALERTA";
  Serial.print("[t="); Serial.print(ahora);
  Serial.print("ms] Temp: "); Serial.print(temperatura,1);
  Serial.print("C | Hum: "); Serial.print(humedad,1);
  Serial.print("% | Estado: "); Serial.println(est);
}

// ── MÁQUINA DE ESTADOS
//Umbral: 30°c
//ALARMA: La temperatura es mayor que el umbral. Prende el LED de color rojo y el buzzer da 3 pitidos   
//ADVERTENCIA: La temperatura está 2°c mas bajo del umbral. El LED prende de color amarillo, sin sonido de buzzer
//NORMAL: La temperatura es menor que el umbral. El LED prende de color Verde. sin sonido de buzzer
void actualizarEstado() {
  int nuevo;
  if      (temperatura >= umbralTemp)     nuevo = ALARMA;
  else if (temperatura >= umbralTemp - 2) nuevo = ADVERTENCIA;
  else                                    nuevo = NORMAL;

  if (nuevo != estadoPrev) {
    //Transicion de ALARMA a NORMAL: 1 pitido largo del buzzer
    if (nuevo == ALARMA)                              iniciarBuzzer(3, false);
    else if (nuevo == NORMAL && estadoPrev == ALARMA) iniciarBuzzer(1, true);
    estadoPrev = nuevo;
  }
  estado = nuevo;
}

// ── LED RGB
void actualizarLED() {
  switch(estado) {
    case NORMAL:      setRGB(0,1,0); break;  // verde
    case ADVERTENCIA: setRGB(1,1,0); break;  // amarillo
    case ALARMA:      setRGB(1,0,0); break;  // rojo
  }
}

// ── BUZZER no bloqueante
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
  unsigned long durOFF = buzzerLargo ? 200 : 150;
  unsigned long dur    = (buzzerPaso % 2 == 0) ? durON : durOFF;
  if (ahora - tBuzzer >= dur) {
    tBuzzer = ahora;
    if (buzzerPaso % 2 == 0) tone(BUZZER, buzzerLargo ? 880 : 1400);
    else                      noTone(BUZZER);
    buzzerPaso++;
    if (buzzerPaso >= buzzerTotal) { noTone(BUZZER); buzzerON = false; }
  }
}

// ── OLED
void actualizarOLED(unsigned long ahora) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(20, 0);
  display.println("TEMP & HUMEDAD");
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 13);
  display.print(temperatura, 1);
  display.println("C");

  display.setTextSize(1);
  display.setCursor(0, 33);
  display.print("Hum: "); display.print(humedad,1); display.println("%");

  display.setCursor(0, 43);
  display.print("Umbral: "); display.print(umbralTemp); display.println("C");

  display.setCursor(0, 53);
  display.print("Estado: ");
  switch(estado) {
    case NORMAL:      display.print("NORMAL");      break;
    case ADVERTENCIA: display.print("ADVERTENCIA"); break;
    case ALARMA:      display.print("ALERTA!");     break;
  }
  display.display();
}