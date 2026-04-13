#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>

#define PIN_DHT     16
#define PIN_CS_SD   4
#define PIN_BTN     26
#define PIN_LED_R   13
#define PIN_LED_G   12
#define PIN_LED_B   14
#define PIN_BUZZER  27

DHT dht(PIN_DHT, DHT22);
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

enum Estado { MONITOR, ALERTA, LOG };
Estado estado_actual = MONITOR;

float temp_c = 0, humedad = 0;
unsigned long t_ultimo_guardado = 0;
unsigned long t_btn_presionado  = 0;
int total_registros = 0;
bool oled_ok = false;
const int INTERVALO_GUARDADO = 10000;

void setLED(bool r, bool g, bool bl) {
  digitalWrite(PIN_LED_R, r  ? HIGH : LOW);
  digitalWrite(PIN_LED_G, g  ? HIGH : LOW);
  digitalWrite(PIN_LED_B, bl ? HIGH : LOW);
}

void guardarEnSD() {
  File f = SD.open("/datos.csv", FILE_APPEND);
  if (f) {
    f.print(millis() / 1000); f.print(",");
    f.print(temp_c, 1);       f.print(",");
    f.println(humedad, 1);
    f.close();
    total_registros++;
    Serial.print("Guardado #"); Serial.println(total_registros);
  }
}

void mostrarOLED() {
  if (!oled_ok) return;
  oled.clearDisplay();
  oled.setTextColor(WHITE);
  oled.setTextSize(1);

  switch (estado_actual) {
    case MONITOR:
      oled.setCursor(0, 0);  oled.print("MONITOR");
      oled.setTextSize(2);
      oled.setCursor(0, 16); oled.print(temp_c, 1); oled.print("C");
      oled.setTextSize(1);
      oled.setCursor(0, 40); oled.print("Hum: "); oled.print(humedad, 1); oled.print("%");
      oled.setCursor(0, 54); oled.print("Reg: "); oled.print(total_registros);
      break;
    case ALERTA:
      oled.setCursor(20, 0);  oled.print("!!! ALERTA !!!");
      oled.setTextSize(2);
      oled.setCursor(0, 16); oled.print(temp_c, 1); oled.print("C");
      oled.setTextSize(1);
      oled.setCursor(0, 42);
      if (temp_c > 35)  oled.print("TEMPERATURA ALTA");
      if (humedad > 85) oled.print("HUMEDAD ALTA");
      break;
    case LOG:
      oled.setCursor(10, 10); oled.print("MODO REGISTRO");
      oled.setCursor(10, 28); oled.print("Total: ");
      oled.setTextSize(2);
      oled.setCursor(10, 40); oled.print(total_registros);
      oled.setTextSize(1);
      oled.setCursor(0, 58);  oled.print("Suelta el boton");
      break;
  }
  oled.display();
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  setLED(false, false, false);

  pinMode(PIN_CS_SD, OUTPUT);
  digitalWrite(PIN_CS_SD, HIGH);

  Wire.begin(21, 22);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Error: OLED no encontrada.");
  } else {
    oled_ok = true;
    Serial.println("OLED lista.");
    oled.clearDisplay();
    oled.setTextColor(WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0); oled.print("Iniciando...");
    oled.display();
  }

  dht.begin();
  pinMode(PIN_BTN, INPUT_PULLUP);

  if (SD.begin(PIN_CS_SD)) {
    Serial.println("SD lista.");
  } else {
    Serial.println("Error: SD no encontrada.");
  }

  Serial.println("Sistema listo.");
}

void loop() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) { humedad = h; temp_c = t; }

  bool btn = (digitalRead(PIN_BTN) == LOW);
  if (btn) {
    if (t_btn_presionado == 0) t_btn_presionado = millis();
    if (millis() - t_btn_presionado > 3000) estado_actual = LOG;
  } else {
    t_btn_presionado = 0;
    if (estado_actual == LOG) estado_actual = MONITOR;
  }

  if (estado_actual != LOG) {
    if (temp_c > 35.0 || humedad > 85.0)
      estado_actual = ALERTA;
    else if (estado_actual == ALERTA)
      estado_actual = MONITOR;
  }

  switch (estado_actual) {
    case MONITOR:
      setLED(false, true, false);
      Serial.print("MONITOR | Temp: "); Serial.print(temp_c);
      Serial.print("C | Hum: "); Serial.print(humedad);
      Serial.print("% | Reg: "); Serial.println(total_registros);
      if (millis() - t_ultimo_guardado >= INTERVALO_GUARDADO) {
        guardarEnSD();
        t_ultimo_guardado = millis();
      }
      break;
    case ALERTA:
      setLED(true, false, false);
      tone(PIN_BUZZER, 1500, 300);
      Serial.print("!!! ALERTA !!! Temp: "); Serial.print(temp_c);
      Serial.print("C | Hum: "); Serial.println(humedad);
      break;
    case LOG:
      setLED(false, false, true);
      Serial.print("MODO LOG | Total: "); Serial.println(total_registros);
      break;
  }

  mostrarOLED();
  delay(500);
}