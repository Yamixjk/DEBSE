#include <SPI.h>
#include <SD.h>

const int PIN_CS_SD = 10;

void setup() {
  Serial.begin(9600);
  Serial.print("Inicializando SD...");

  if (!SD.begin(PIN_CS_SD)) {
    Serial.println(" ERROR. Verifica conexiones y formato FAT32.");
    return;
  }
  Serial.println(" OK");

  // Listar archivos en la raiz
  File root = SD.open("/");
  Serial.println("Archivos en la SD:");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    Serial.print("  ");
    Serial.print(entry.name());
    Serial.print("  ");
    Serial.println(entry.size());
    entry.close();
  }

  // Crear y escribir archivo de prueba
  File archivo = SD.open("TEST.TXT", FILE_WRITE);
  if (archivo) {
    archivo.println("Lab 08 - Prueba de escritura SD");
    archivo.println("Sistema embebido OK");
    archivo.close();
    Serial.println("Archivo TEST.TXT creado correctamente.");
  }
}

void loop() {}