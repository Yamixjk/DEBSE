const int PIN_BUZZER = 6;

int notas[] = {262, 294, 330, 349, 392, 440, 494};

void setup() {
  Serial.begin(9600);
}

void loop() {
  for (int i = 0; i < 7; i++) {
    tone(PIN_BUZZER, notas[i], 300);
    delay(350);
  }

  noTone(PIN_BUZZER);
  delay(1000);
}