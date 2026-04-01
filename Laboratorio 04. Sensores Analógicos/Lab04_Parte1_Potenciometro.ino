// Lab 04 - Parte 1: Potenciómetro - Divisor de voltaje variable
// Wokwi: Potenciómetro en A0, LED con PWM en pin 9

const int PIN_POT = A0;   // Pin analógico
const int PIN_LED = 9;    // LED con PWM

void setup() {
  Serial.begin(9600);
  Serial.println("Lectura\tVoltaje\tPorcentaje\tPWM");
}

void loop() {
  int lectura    = analogRead(PIN_POT);              // 0 - 1023
  float voltaje  = lectura * (5.0 / 1023.0);        // 0.0 - 5.0 V
  int porcentaje = map(lectura, 0, 1023, 0, 100);   // 0 - 100 %
  int pwm_val    = map(lectura, 0, 1023, 0, 255);   // 0 - 255

  // Controlar brillo del LED con el potenciómetro
  analogWrite(PIN_LED, pwm_val);

  // Imprimir tabla de valores (compatible con Serial Plotter)
  Serial.print(lectura);      Serial.print('\t');
  Serial.print(voltaje, 2);   Serial.print('\t');
  Serial.print(porcentaje);   Serial.print('\t');
  Serial.println(pwm_val);

  delay(100);
}
