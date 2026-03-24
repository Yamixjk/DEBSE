const int pinBoton = 2;
const int pinReset = 3;
const int pinLED = 13;

int conteo = 0;

void setup() {
  pinMode(pinBoton, INPUT_PULLUP);
  pinMode(pinReset, INPUT_PULLUP);
  pinMode(pinLED, OUTPUT);

  Serial.begin(9600);
  Serial.println("Modo INPUT_PULLUP Activo");
}

void loop() {
  int lecturaBoton = digitalRead(pinBoton);
  int lecturaReset = digitalRead(pinReset);

  // Botón principal (conteo + parpadeo)
  if (lecturaBoton == LOW) {
    conteo++;
    Serial.print("Pulsacion #");
    Serial.println(conteo);

    // Parpadeo según el conteo
    for (int i = 0; i < conteo; i++) {
      digitalWrite(pinLED, HIGH);
      delay(300);
      digitalWrite(pinLED, LOW);
      delay(300);
    }

    delay(300); // evitar múltiples lecturas
  }

  // Botón de reset
  if (lecturaReset == LOW) {
    conteo = 0;
    Serial.println("CONTEO REINICIADO");
    delay(300);
  }
}
