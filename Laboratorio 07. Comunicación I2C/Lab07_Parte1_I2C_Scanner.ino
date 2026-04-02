// Lab 07 - Parte 1: Escaneo del bus I2C
// Detecta las direcciones de todos los dispositivos I2C conectados

#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(9600);
  Serial.println("Escaneando bus I2C...");

  int dispositivos = 0;

  for (byte addr = 8; addr < 120; addr++) {
    Wire.beginTransmission(addr);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Dispositivo encontrado en: 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      dispositivos++;
    }
  }

  Serial.print("Total de dispositivos: ");
  Serial.println(dispositivos);
}

void loop() {}
