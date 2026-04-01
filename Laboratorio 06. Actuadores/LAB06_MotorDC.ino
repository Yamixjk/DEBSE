const int ENA = 10;
const int IN1 = 8;
const int IN2 =9;

void motorAdelante(int velocidad){
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, velocidad);
}

void motorAtras(int velocidad){
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  analogWrite(ENA, velocidad);
}

void motorFreno(){
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, HIGH);
  analogWrite(ENA, 255);
}

void motorLibre(){
  analogWrite(ENA, 0);
}

void setup() {
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  Serial.begin(9600);
  Serial.println("LAB06 - Control Motor DC");
}

void loop() {
  Serial.println("Adelante: velocidad creciente...");

  for(int v=100; v<= 255; v+=10){
    motorAdelante(v);
    Serial.print("Velocidad: ");
    Serial.println(v);
    delay(300);
  }

  motorLibre();
  delay(500);

  Serial.println("Atras: velocidad media...");
  motorAtras(180);
  delay(2000);

  motorFreno();
  delay(500);

  delay(1000);

}
