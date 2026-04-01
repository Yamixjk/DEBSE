#include <Servo.h>

Servo mi_servo;

const int PIN_SERVO = 11;
const int PIN_POT   = A0;

int angulo_anterior = -1;

void setup() {
  mi_servo.attach(PIN_SERVO);
  mi_servo.write(90);

  Serial.begin(9600);
  Serial.println("Servo listo. Gira el potenciometro.");
  Serial.println("Pot_ADC\tAngulo");
}

void loop() {
  int pot_val = analogRead(PIN_POT);
  int angulo = map(pot_val, 0, 1023, 0, 180);

  if (abs(angulo - angulo_anterior) >= 2) {
    mi_servo.write(angulo);
    angulo_anterior = angulo;

    Serial.print(pot_val);
    Serial.print("\t");
    Serial.println(angulo);
  }

  delay(20);
}