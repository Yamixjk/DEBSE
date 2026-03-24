const int pinLED = 13;
int contador;

void setup() {
  pinMode(pinLED, OUTPUT);
  Serial.begin(9600);
  Serial.println("Contador - Ej3");
}

void loop() {
  contador++;

  // Guardamos el tiempo actual en milisegundos
  unsigned long tiempoActual = millis();

  // Encendido
  digitalWrite(pinLED, HIGH);
  Serial.print("[#");
  Serial.print(contador);
  Serial.print("] t=");
  Serial.print(tiempoActual);
  Serial.println(" ms LED -> ENCENDIDO");
  delay(500);

  // Actualizamos el tiempo para el siguiente mensaje
  tiempoActual = millis();

  // Apagado
  digitalWrite(pinLED, LOW);
  Serial.print("[#");
  Serial.print(contador);
  Serial.print("] t=");
  Serial.print(tiempoActual);
  Serial.println(" ms LED -> APAGADO");
  delay(500);
}
