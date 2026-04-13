// ============================================================
// PROYECTO INTEGRADOR 4 — Estación Meteorológica con Datalogger
// Plataforma: ESP32 | Simulación: Wokwi
// LED RGB cátodo común | BMP280
// ============================================================

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <DHT.h>
#include <Adafruit_BMP280.h>        // ← cambiado
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// ── PINES
#define PIN_DHT    15
#define PIN_LDR    34
#define PIN_CS_SD  5
#define PIN_BTN    26
#define PIN_LED_R  13
#define PIN_LED_G  12
#define PIN_LED_B  14

// ── OBJETOS
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
Adafruit_BMP280  bmp;               // ← cambiado
DHT dht(PIN_DHT, DHT22);

// ── VARIABLES SENSORES
float temp_dht  = 0.0;
float hum_dht   = 0.0;
float temp_bmp  = 0.0;
float presion   = 0.0;
float altitud   = 0.0;
int   luz_pct   = 0;

// ── CONTROL
bool  grabando       = false;
int   pantallaActual = 0;
int   totalRegistros = 0;
bool  oled_ok        = false;
bool  bmp_ok         = false;
bool  sd_ok          = false;
bool  errorSensor    = false;

// ── TEMPORIZADORES
unsigned long tSensores = 0;
unsigned long tLDR      = 0;
unsigned long tPantalla = 0;
unsigned long tSerial   = 0;
unsigned long tDebBtn   = 0;

// ── LED RGB
void setLED(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_R, r);
  digitalWrite(PIN_LED_G, g);
  digitalWrite(PIN_LED_B, b);
}

// ── SETUP
void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_BTN,   INPUT_PULLUP);
  setLED(0, 0, 0);

  Wire.begin(21, 22);

  // OLED
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Error OLED");
  } else {
    oled_ok = true;
    oled.clearDisplay();
    oled.setTextColor(WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println("Iniciando...");
    oled.display();
  }

  // BMP280 — dirección I2C por defecto: 0x76 (también puede ser 0x77)
  if (!bmp.begin(0x76)) {           // ← cambiado (BMP280 usa begin con dirección)
    Serial.println("Error BMP280");
    bmp_ok = false;
  } else {
    bmp_ok = true;
    Serial.println("BMP280 listo");

    // Configuración recomendada para estación meteorológica
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,   // temperatura
                    Adafruit_BMP280::SAMPLING_X16,  // presión
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
  }

  // DHT
  dht.begin();

  // SD
  if (!SD.begin(PIN_CS_SD)) {
    Serial.println("Error SD");
    sd_ok = false;
  } else {
    sd_ok = true;
    Serial.println("SD lista");
    if (!SD.exists("/DATA.CSV")) {
      File f = SD.open("/DATA.CSV", FILE_WRITE);
      if (f) {
        f.println("tiempo_ms,temp_dht,hum,temp_bmp,presion,luz_pct");
        f.close();
      }
    }
  }

  delay(1000);
}

// ── LOOP
void loop() {
  unsigned long ahora = millis();
  leerBoton(ahora);
  leerSensores(ahora);
  leerLDR(ahora);
  actualizarLED();
  rotarPantalla(ahora);
  mostrarOLED();
  enviarSerial(ahora);
  if (grabando) guardarSD(ahora);
}

// ── BOTÓN toggle GRABANDO/PAUSADO
void leerBoton(unsigned long ahora) {
  if (ahora - tDebBtn < 200) return;
  if (digitalRead(PIN_BTN) == LOW) {
    grabando = !grabando;
    tDebBtn  = ahora;
    Serial.println(grabando ? ">> GRABANDO" : ">> PAUSADO");
  }
}

// ── LECTURA DHT22 + BMP280 cada 5 s
void leerSensores(unsigned long ahora) {
  if (ahora - tSensores < 5000) return;
  tSensores = ahora;

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    errorSensor = true;
  } else {
    errorSensor = false;
    temp_dht = t;
    hum_dht  = h;
  }

  if (bmp_ok) {
    temp_bmp = bmp.readTemperature();
    presion  = bmp.readPressure() / 100.0F;          // Pa → hPa (igual que antes)
    altitud  = bmp.readAltitude(1013.25);             // ← cambiado: hPa en vez de Pa
  }
}

// ── LECTURA LDR cada 1 s
void leerLDR(unsigned long ahora) {
  if (ahora - tLDR < 1000) return;
  tLDR = ahora;
  int raw = analogRead(PIN_LDR);
  luz_pct = map(raw, 0, 4095, 0, 100);
}

// ── LED RGB según estado
void actualizarLED() {
  if (errorSensor || !sd_ok) {
    setLED(1, 0, 0);  // rojo = error
  } else if (grabando) {
    setLED(0, 0, 1);  // azul = grabando
  } else {
    setLED(0, 1, 0);  // verde = OK
  }
}

// ── ROTACIÓN DE PANTALLAS cada 3 s
void rotarPantalla(unsigned long ahora) {
  if (ahora - tPantalla < 3000) return;
  tPantalla = ahora;
  pantallaActual = (pantallaActual + 1) % 3;
}

// ── OLED
void mostrarOLED() {
  if (!oled_ok) return;
  oled.clearDisplay();
  oled.setTextColor(WHITE);
  oled.setTextSize(1);

  switch (pantallaActual) {

    // Pantalla 1: Temp + Hum
    case 0:
      oled.setCursor(20, 0);
      oled.println("TEMP + HUMEDAD");
      oled.drawLine(0, 9, 127, 9, WHITE);
      oled.setTextSize(2);
      oled.setCursor(0, 14);
      oled.print(temp_dht, 1); oled.println("C");
      oled.setTextSize(1);
      oled.setCursor(0, 38);
      oled.print("Hum: "); oled.print(hum_dht, 1); oled.println("%");
      oled.setCursor(0, 50);
      oled.print(grabando ? ">> GRABANDO" : "   PAUSADO");
      break;

    // Pantalla 2: Presión + Altitud
    case 1:
      oled.setCursor(10, 0);
      oled.println("PRESION + ALTITUD");
      oled.drawLine(0, 9, 127, 9, WHITE);
      oled.setCursor(0, 14);
      oled.print("Presion: "); oled.print(presion, 1); oled.println("hPa");
      oled.setCursor(0, 26);
      oled.print("Altitud: "); oled.print(altitud, 1); oled.println("m");
      oled.setCursor(0, 38);
      oled.print("TempBMP: "); oled.print(temp_bmp, 1); oled.println("C");
      oled.setCursor(0, 50);
      oled.print(grabando ? ">> GRABANDO" : "   PAUSADO");
      break;

    // Pantalla 3: Luz + Estado
    case 2:
      oled.setCursor(25, 0);
      oled.println("LUZ + ESTADO");
      oled.drawLine(0, 9, 127, 9, WHITE);
      oled.setCursor(0, 14);
      oled.print("Luz: "); oled.print(luz_pct); oled.println("%");
      oled.drawRect(0, 26, 128, 10, WHITE);
      oled.fillRect(1, 27, map(luz_pct, 0, 100, 0, 126), 8, WHITE);
      oled.setCursor(0, 40);
      oled.print("Registros: "); oled.println(totalRegistros);
      oled.setCursor(0, 50);
      oled.print(errorSensor ? "ERROR SENSOR" : grabando ? ">> GRABANDO" : "   PAUSADO");
      break;
  }
  oled.display();
}

// ── SERIAL cada 5 s
void enviarSerial(unsigned long ahora) {
  if (ahora - tSerial < 5000) return;
  tSerial = ahora;
  Serial.print("Temp_DHT: "); Serial.print(temp_dht, 1);
  Serial.print("C | Hum: ");  Serial.print(hum_dht, 1);
  Serial.print("% | Temp_BMP: "); Serial.print(temp_bmp, 1);
  Serial.print("C | Presion: "); Serial.print(presion, 1);
  Serial.print("hPa | Alt: "); Serial.print(altitud, 1);
  Serial.print("m | Luz: "); Serial.print(luz_pct);
  Serial.print("% | "); Serial.println(grabando ? "GRABANDO" : "PAUSADO");
}

// ── GUARDAR EN SD
void guardarSD(unsigned long ahora) {
  static unsigned long tGuardado = 0;
  if (ahora - tGuardado < 5000) return;
  tGuardado = ahora;
  if (!sd_ok) return;
  File f = SD.open("/DATA.CSV", FILE_APPEND);
  if (f) {
    f.print(ahora);       f.print(",");
    f.print(temp_dht, 1); f.print(",");
    f.print(hum_dht, 1);  f.print(",");
    f.print(temp_bmp, 1); f.print(",");
    f.print(presion, 1);  f.print(",");
    f.println(luz_pct);
    f.close();
    totalRegistros++;
  }
}