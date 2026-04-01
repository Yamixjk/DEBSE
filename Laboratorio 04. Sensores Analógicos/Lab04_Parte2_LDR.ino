// Lab 04 - Parte 2: LDR - Sensor de luz
// Wokwi: LDR (photoresistor sensor) en A1, LED PWM en pin 9, Buzzer en pin 6

const int PIN_LDR    = A1;
const int PIN_LED    = 9;
const int PIN_BUZZER = 6;

// Calibración: ajusta estos valores según tu LDR y condiciones de luz
const int LDR_OSCURO = 800;  // Valor ADC con luz apagada
const int LDR_LUZ    = 100;  // Valor ADC con luz directa

void setup() {
  Serial.begin(9600);
  Serial.println("LDR_raw\tLuz%\tEstado");
}

void loop() {
  int lectura_ldr = analogRead(PIN_LDR);

  // map() puede recibir rangos invertidos (oscuro=800 -> 0%, luz=100 -> 100%)
  int luz_pct = map(lectura_ldr, LDR_OSCURO, LDR_LUZ, 0, 100);
  luz_pct     = constrain(luz_pct, 0, 100);   // Limitar a 0-100%

  // LED inversamente proporcional a la luz (lámpara automática)
  int brillo_led = map(luz_pct, 0, 100, 255, 0);
  analogWrite(PIN_LED, brillo_led);

  // Alarma si hay muy poca luz
  String estado;
  if (luz_pct < 20) {
    estado = "MUY OSCURO - ALARMA";
    tone(PIN_BUZZER, 1000, 100);
  } else if (luz_pct < 50) {
    estado = "Penumbra";
  } else {
    estado = "Iluminado";
  }

  Serial.print(lectura_ldr);  Serial.print('\t');
  Serial.print(luz_pct);      Serial.print('\t');
  Serial.println(estado);
  delay(200);
}
