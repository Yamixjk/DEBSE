// Lab 07 - Parte 2: Lectura BMP180 + Display OLED SSD1306
// Muestra temperatura, presión y altitud en pantalla OLED

#include <Wire.h>
#include <Adafruit_BMP085.h>       // librería BMP180 (compatible con BMP085)
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define OLED_W    128
#define OLED_H     64
#define OLED_ADDR 0x3C

Adafruit_BMP085 bmp;
Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);

// Presión al nivel del mar (ajústala a tu ciudad)
const float PRESION_MAR = 101325;  // Pa

void setup() {
  Serial.begin(9600);

  // --- Inicializar OLED ---
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("ERROR: OLED no encontrada");
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(10, 30);
  display.println("Iniciando BMP180...");
  display.display();

  // --- Inicializar BMP180 ---
  if (!bmp.begin()) {
    Serial.println("ERROR: BMP180 no encontrado");
    display.clearDisplay();
    display.setCursor(10, 30);
    display.println("BMP180 no hallado");
    display.display();
    while (true);
  }

  Serial.println("BMP180 y OLED listos");
  Serial.println("Tiempo\tTemp(C)\tPresion(hPa)\tAltitud(m)");
}

int segundos = 0;

void loop() {
  float temp    = bmp.readTemperature();
  float presion = bmp.readPressure() / 100.0F;      // Pa -> hPa
  float altitud = bmp.readAltitude(PRESION_MAR);

  // --- Serial Monitor ---
  Serial.print(segundos);   Serial.print('\t');
  Serial.print(temp, 2);    Serial.print('\t');
  Serial.print(presion, 2); Serial.print('\t');
  Serial.println(altitud, 1);

  // --- Pantalla OLED ---
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(25, 0);
  display.println("-- BMP180 --");

  display.setTextSize(2);
  display.setCursor(0, 16);
  display.print(temp, 1);
  display.println(" C");

  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print("P: ");
  display.print(presion, 1);
  display.println(" hPa");

  display.setCursor(0, 52);
  display.print("Alt: ");
  display.print(altitud, 0);
  display.println(" m");

  display.display();

  segundos++;
  delay(1000);
}
