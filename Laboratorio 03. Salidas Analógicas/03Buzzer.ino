#include <math.h>

const int pinLED = 9;
float valor;
int brillo;

void setup(){
  pinMode(pinLED, OUTPUT);
}

void loop(){
  for (int x = 0; x < 360; x++){
    valor = sin(x * (M_PI/180.0));
    brillo = int (127.5 + (127.5 * valor));
  

  analogWrite(pinLED, brillo);
  delay(5);
  }
}