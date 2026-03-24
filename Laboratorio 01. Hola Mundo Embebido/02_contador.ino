const int pinLED = 13;
int contador = 0;

void setup() {
  pinMode(pinLED, OUTPUT);
  Serial.begin(9600);
  Serial.println("Laboratorio 01 - Ej1");
}

void loop() {
  contador++;

  digitalWrite(pinLED, HIGH);
  Serial.print("[#");
  Serial.print(contador);
  Serial.println("] LED -> Encendido");
  delay(200);

  digitalWrite(pinLED, LOW);
  Serial.print("[#");
  Serial.print(contador);
  Serial.println("] LED -> Apagado");
  delay(800);
}
