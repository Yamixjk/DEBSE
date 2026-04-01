#include <DHT.h>

#define PIN_DHT 7
#define TIPO_DHT DHT22 // Usar DHT11 si tienes ese modelo
#define PIN_LED_VERDE 8
#define PIN_LED_ROJO 9
#define PIN_BUZZER 6

DHT dht(PIN_DHT, TIPO_DHT);

// Umbrales de alerta
const float TEMP_ALTA = 30.0; // Celsius
const float TEMP_BAJA = 10.0;
const float HUM_ALTA = 80.0; // Porcentaje

void setup() {
  Serial.begin(9600);
  dht.begin();

  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_ROJO, OUTPUT);

  Serial.println("Tiempo(s)\tTemp(C)\tTemp(F)\tHumedad%\tHeatIdx\tEstado");
}

void loop() {
  delay(2000); // DHT22: esperar al menos 2 s entre lecturas

  float humedad = dht.readHumidity();
  float temp_c = dht.readTemperature();
  float temp_f = dht.readTemperature(true); // true = Fahrenheit

  // Verificar si la lectura fue válida
  if (isnan(humedad) || isnan(temp_c)) {
    Serial.println("ERROR: Fallo en lectura DHT22");
    digitalWrite(PIN_LED_ROJO, HIGH);
    return;
  }

  // Índice de calor (sensación térmica)
  float heat_idx = dht.computeHeatIndex(temp_c, humedad, false);

  // Determinar estado y actuar
  String estado;

  if (temp_c > TEMP_ALTA || humedad > HUM_ALTA) {
    estado = "ALERTA-CALOR";
    digitalWrite(PIN_LED_ROJO, HIGH);
    digitalWrite(PIN_LED_VERDE, LOW);
    tone(PIN_BUZZER, 2000, 200);

  } else if (temp_c < TEMP_BAJA) {
    estado = "ALERTA-FRIO";
    digitalWrite(PIN_LED_ROJO, HIGH);
    digitalWrite(PIN_LED_VERDE, LOW);
    tone(PIN_BUZZER, 500, 500);

  } else {
    estado = "NORMAL";
    digitalWrite(PIN_LED_VERDE, HIGH);
    digitalWrite(PIN_LED_ROJO, LOW);
  }

  // Imprimir datos
  Serial.print(millis() / 1000); Serial.print('\t');
  Serial.print(temp_c, 1); Serial.print('\t');
  Serial.print(temp_f, 1); Serial.print('\t');
  Serial.print(humedad, 1); Serial.print('\t');
  Serial.print(heat_idx, 1); Serial.print('\t');
  Serial.println(estado);
}