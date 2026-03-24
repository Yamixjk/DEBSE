const int pinPot = A0;
const int pinLED = 9;

void setup(){
  pinMode(pinLED, OUTPUT);
  Serial.begin(9600);
  Serial.println("Lab 03 - Control de Brillo con Potenciometro");
}

void loop(){
  int lectura = analogRead(pinPot);
  int brillo = map(lectura, 0, 1023, 0, 255);
  analogWrite(pinLED, brillo);

  Serial.print("Lectura ADC: ");
  Serial.print(lectura);
  Serial.print(" | Brillo PMW: ");
  Serial.println(brillo);

  delay(10);
}