const int pinBoton = 2;
const int pinLED = 13;

void setup() {
// Se activa la resistencia interna de 20k-50k ohm
pinMode(pinBoton, INPUT_PULLUP);
pinMode(pinLED, OUTPUT);

Serial.begin(9600);
Serial.println("Modo INPUT_PULLUP Activo");
}

void loop() {
// Leemos el estado
int lectura = digitalRead(pinBoton);

// Si la lectura es LOW, significa que el botón está presionado
if (lectura == LOW) {
digitalWrite(pinLED, HIGH);
Serial.println("Boton presionado (Estado: LOW)");
} else {
digitalWrite(pinLED, LOW);
}
}