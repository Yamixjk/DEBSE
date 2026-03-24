const int pinLED = 13;

void setup() {
  pinMode(pinLED, OUTPUT);
  Serial.begin(9600);
  Serial.println("Sistema SOS - Ej2");
}

void loop() {
  // Letra S (3 puntos)
  Serial.println("Enviando: S");
  for (int i = 0; i < 3; i++) {
    digitalWrite(pinLED, HIGH);
    delay(200);
    digitalWrite(pinLED, LOW);
    delay(200);
  }
  delay(400);

  // Letra 0 (3 rayas)
  Serial.println("Enviando: 0");
  for (int i = 0; i < 3; i++) {
    digitalWrite(pinLED, HIGH);
    delay(600);
    digitalWrite(pinLED, LOW);
    delay(200);
  }
  delay(400);

  // Letra S (3 puntos)
  Serial.println("Enviando: S");
  for (int i = 0; i < 3; i++) {
    digitalWrite(pinLED, HIGH);
    delay(200);
    digitalWrite(pinLED, LOW);
    delay(200);
  }

  Serial.println("Pausa de ciclo ... ");
  delay(2000);
}
