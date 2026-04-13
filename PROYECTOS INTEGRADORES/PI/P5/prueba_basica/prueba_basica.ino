// ============================================================
//  PRUEBA BÁSICA — Sensores IR + L298N + LED
//  Avanza si ambos sensores ven negro, se detiene si no.
//  LED encendido = sistema activo.
// ============================================================

// --- PINES L298N ---
#define ENA 3    // PWM motor izquierdo
#define IN1 7
#define IN2 8
#define ENB 5    // PWM motor derecho
#define IN3 12
#define IN4 13

// --- PINES SENSORES IR ---
#define SENSOR_IZQ A0
#define SENSOR_DER A1

// --- OTROS ---
#define PIN_LED 4

// --- CALIBRACIÓN ---
// Si el sensor devuelve valor ALTO sobre negro, deja > 500
// Si devuelve valor BAJO sobre negro, cambia a < 500
#define UMBRAL 500

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(PIN_LED, OUTPUT);

  digitalWrite(PIN_LED, HIGH);  // LED siempre encendido en prueba

  Serial.begin(9600);
  Serial.println("Prueba iniciada.");
}

void loop() {
  int valIzq = analogRead(SENSOR_IZQ);
  int valDer = analogRead(SENSOR_DER);

  // Muestra valores para calibrar el umbral
  Serial.print("IZQ: ");
  Serial.print(valIzq);
  Serial.print("  |  DER: ");
  Serial.println(valDer);

  bool negroIzq = (valIzq > UMBRAL);
  bool negroDer = (valDer > UMBRAL);

  if (negroIzq && negroDer) {
    avanzar();
  } else {
    detener();
  }

  delay(50);
}

void avanzar() {
  // Motor izquierdo adelante
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 140);   // velocidad moderada

  // Motor derecho adelante
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENB, 140);
}

void detener() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}
