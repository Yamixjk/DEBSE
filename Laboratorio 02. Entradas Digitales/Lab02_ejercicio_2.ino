const int botonA = 2;
const int botonB = 3;

const int ledVerde = 9;
const int ledRojo = 10;

void setup() {
  pinMode(botonA, INPUT_PULLUP);
  pinMode(botonB, INPUT_PULLUP);

  pinMode(ledVerde, OUTPUT);
  pinMode(ledRojo, OUTPUT);
}

void loop() {
  int estadoA = digitalRead(botonA);
  int estadoB = digitalRead(botonB);

  // Botón A controla LED verde
  if (estadoA == LOW) {
    digitalWrite(ledVerde, HIGH);
  } else {
    digitalWrite(ledVerde, LOW);
  }

  // Botón B controla LED rojo
  if (estadoB == LOW) {
    digitalWrite(ledRojo, HIGH);
  } else {
    digitalWrite(ledRojo, LOW);
  }
}
