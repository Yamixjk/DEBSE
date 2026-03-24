// Lab02_Pulsador_Basico.ino
const int PIN_BOTON = 2; // Entrada digital
const int PIN_LED = 8; // Salida digital
void setup() {
  pinMode(PIN_BOTON, INPUT); // Entrada normal (requiere R pull-down externa)
  pinMode(PIN_LED, OUTPUT);
  Serial.begin(115200);
  Serial.println("Lab 02 — Lectura de pulsador");
}
void loop() {
  int estado = digitalRead(PIN_BOTON); // Lee HIGH o LOW
  if (estado == HIGH) {
    digitalWrite(PIN_LED, HIGH); // LED encendido
    Serial.println("Boton: PRESIONADO");
  } else {
    digitalWrite(PIN_LED, LOW); // LED apagado
  }
  delay(50);
}