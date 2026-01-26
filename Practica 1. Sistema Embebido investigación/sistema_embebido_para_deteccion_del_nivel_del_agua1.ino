#include <LiquidCrystal.h>

LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

int sensor = A0;

int verde    = 8;
int amarillo = 9;
int rojo     = 10;
int buzzer   = 11;
int bomba    = 12;

void apagarTodo() {
  digitalWrite(verde, LOW);
  digitalWrite(amarillo, LOW);
  digitalWrite(rojo, LOW);
  digitalWrite(buzzer, LOW);
  digitalWrite(bomba, LOW);
}

void setup() {
  pinMode(verde, OUTPUT);
  pinMode(amarillo, OUTPUT);
  pinMode(rojo, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(bomba, OUTPUT);

  lcd.begin(16, 2);
  lcd.print("Control de Agua");
  delay(1500);
  lcd.clear();
}

void loop() {
  int nivel = analogRead(sensor);

  apagarTodo();

  lcd.setCursor(0, 0);
  lcd.print("Nivel: ");
  lcd.print(nivel);
  lcd.print("   ");

  if (nivel < 350) {
    digitalWrite(verde, HIGH);   // Nivel bajo
    digitalWrite(bomba, HIGH);   // Llenando
    lcd.setCursor(0, 1);
    lcd.print("Estado: LLENAR ");
  }
  else if (nivel < 700) {
    digitalWrite(amarillo, HIGH);// Nivel medio
    lcd.setCursor(0, 1);
    lcd.print("Estado: NORMAL ");
  }
  else {
    digitalWrite(rojo, HIGH);    // Nivel alto
    digitalWrite(buzzer, HIGH);  // Alarma
    lcd.setCursor(0, 1);
    lcd.print("Estado: PELIGRO ");
  }

  delay(500);
}
