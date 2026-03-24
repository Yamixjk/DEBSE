const int pinLED = 13;

void setup() {
  pinMode(pinLED, OUTPUT);

  Serial.begin(9600);
  Serial.println("Laboratorio 01 iniciando...");
}

void loop() {
  digitalWrite(pinLED, HIGH);
  Serial.println("LED Encendido");
  delay(1000);

  digitalWrite(pinLED, LOW);
  Serial.println("LED Apagado");
  delay(1000);
}
