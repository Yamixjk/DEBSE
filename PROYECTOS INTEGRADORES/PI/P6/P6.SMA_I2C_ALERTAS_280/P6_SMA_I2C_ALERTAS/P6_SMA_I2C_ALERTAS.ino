// ============================================================
//  PROYECTO INTEGRADOR 6
//  Sistema de Monitoreo Ambiental con I2C Multi-Sensor
//  y Alertas Inteligentes
//
//  Plataforma : Arduino UNO — Hardware Real
//  Sensor presión/temp : BMP280 (Adafruit_BMP280)
//  Sensor temp/hum     : DHT22
//  CO2 simulado        : Potenciómetro (400–2000 ppm)
//  Pantalla            : OLED SSD1306 128x64 (I2C)
//  Alertas             : LED RGB + Buzzer
//
//  Integrantes (orden alfabético por apellido):
//    1. Castro Márquez      Cristian
//    2. García Segura       América Yanely
//    3. Hernández Ruíz      Haydee Michelle
//    4. Montoya Garza       Luis Ángel
//    5. Navarro García      Maribel
//    6. Rueda Martínez      Alison Michelle
//    7. Silva Sánchez       Yamilka Arely
// ============================================================

// Suprime la pantalla de splash de Adafruit al iniciar
#define SSD1306_NO_SPLASH

// ── LIBRERÍAS ─────────────────────────────────────────────
#include <Wire.h>              // Comunicación I2C (OLED + BMP280)
#include <DHT.h>               // Sensor de temperatura y humedad DHT22
#include <Adafruit_BMP280.h>   // Sensor BMP280 (presión y temperatura)
#include <Adafruit_SSD1306.h>  // Pantalla OLED SSD1306
#include <Adafruit_GFX.h>      // Gráficos base para OLED
#include <Adafruit_Sensor.h>   // Capa unificada de sensores Adafruit

// ── PINES ─────────────────────────────────────────────────
#define PIN_DHT   2    // Señal de datos del DHT22
#define PIN_A     3    // Pulsador A — pantalla siguiente
#define PIN_B     4    // Pulsador B — pantalla anterior
#define PIN_C     5    // Pulsador C — toggle modo CONFIG/MONITOR
#define PIN_BUZ   6    // Buzzer pasivo
#define PIN_R     9    // LED RGB — canal Rojo   (PWM)
#define PIN_G     10   // LED RGB — canal Verde  (PWM)
#define PIN_B_LED 11   // LED RGB — canal Azul   (PWM)
#define PIN_POT   A0   // Potenciómetro — CO2 simulado / umbral en CONFIG

// ── OBJETOS ───────────────────────────────────────────────
DHT              dht(PIN_DHT, DHT22);  // Sensor DHT22
Adafruit_BMP280  bmp;                  // BMP280 (I2C, sin parámetros)
Adafruit_SSD1306 oled(128, 64, &Wire); // OLED 128x64 por I2C

// ── VARIABLES DE SENSORES ─────────────────────────────────
float t_dht   = 0;    // Temperatura leída por el DHT22 (°C)
float hum     = 0;    // Humedad relativa leída por el DHT22 (%)
float t_bmp   = 0;    // Temperatura leída por el BMP280 (°C)
float pres    = 0;    // Presión barométrica en Pascales (Pa)
float co2     = 400;  // CO2 simulado por potenciómetro (ppm)
float ica     = 0;    // Índice de Calidad Ambiental actual (0–100)
float ica_prev= 0;    // ICA de la medición anterior (para tendencia)

// ── VARIABLES DE INTERFAZ ─────────────────────────────────
uint8_t pantalla = 0;      // Pantalla activa: 0=ICA, 1=Temp, 2=Hum+Pres, 3=ICA+CO2
bool    cfg      = false;  // true = modo CONFIGURACIÓN, false = modo MONITOREO
float   umbral   = 28.0;   // Umbral de temperatura configurable (°C)

// ── TIMERS (millis) ───────────────────────────────────────
// Permiten ejecutar tareas periódicas sin usar delay()
unsigned long ts_sensor = 0;  // Última lectura de sensores
unsigned long ts_oled   = 0;  // Último refresco de pantalla
unsigned long ts_serial = 0;  // Último envío JSON por Serial
unsigned long ts_buz    = 0;  // Último pulso del buzzer de alerta
bool buz_on = false;          // Estado actual del buzzer (encendido/apagado)

// ── DEBOUNCE DE BOTONES ───────────────────────────────────
// Técnica: confirmar señal estable durante X ms antes de actuar.
// Botones con pull-down externo (10k a GND): reposo=LOW, presionado=HIGH.
// Se detecta flanco de subida y se espera estabilidad para evitar
// falsas lecturas por rebote mecánico del pulsador.
bool prev_A = false, prev_B = false, prev_C = false;          // Estado anterior de cada botón
bool estable_A = false, estable_B = false, estable_C = false; // Si ya se confirmó la pulsación
unsigned long t_subio_A = 0, t_subio_B = 0, t_subio_C = 0;   // Momento en que subió cada botón
unsigned long db_A = 0, db_B = 0, db_C = 0;                   // Último disparo de cada botón
#define DB_AB  50UL   // 50 ms de estabilidad para A y B (navegación rápida)
#define DB_C  250UL   // 250 ms de estabilidad para C (evitar cambio accidental de modo)


// ============================================================
//  SETUP — Se ejecuta una sola vez al encender o resetear
// ============================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();  // Iniciar bus I2C

  // Configurar pines de botones como entrada simple
  // (la resistencia pull-down de 10k está en el circuito externo)
  pinMode(PIN_A,     INPUT);
  pinMode(PIN_B,     INPUT);
  pinMode(PIN_C,     INPUT);

  // Configurar pines de salida
  pinMode(PIN_BUZ,   OUTPUT);
  pinMode(PIN_R,     OUTPUT);
  pinMode(PIN_G,     OUTPUT);
  pinMode(PIN_B_LED, OUTPUT);

  // Iniciar sensor DHT22
  dht.begin();

  // Iniciar pantalla OLED (dirección I2C 0x3C)
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED fail"));
    while (1);  // Detener ejecución si no responde
  }
  oled.setTextColor(WHITE);
  oled.setTextSize(1);

  // Iniciar sensor BMP280
  // Dirección I2C por defecto: 0x76 (SDO a GND)
  // Si tu módulo usa SDO a VCC, cambia a 0x77
  delay(500);
  if (!bmp.begin(0x76)) {
    Serial.println(F("BMP280 fail"));
  } else {
    // Configuración de oversampling y filtro para lecturas estables
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,    // oversampling temperatura
                    Adafruit_BMP280::SAMPLING_X16,   // oversampling presión
                    Adafruit_BMP280::FILTER_X16,     // filtro IIR
                    Adafruit_BMP280::STANDBY_MS_500);// tiempo entre mediciones
    Serial.println(F("BMP280 OK"));
  }

  // Pantalla de bienvenida en OLED
  oled.clearDisplay();
  oled.setCursor(14, 20); oled.print(F("Monitor Ambiental"));
  oled.setCursor(30, 36); oled.print(F("Iniciando..."));
  oled.display();

  // Tono de arranque: dos pitidos ascendentes
  tone(PIN_BUZ, 1000, 150); delay(200);
  tone(PIN_BUZ, 1400, 150); delay(1300);
}


// ============================================================
//  LOOP — Se ejecuta continuamente
//  Usa millis() para coordinar tareas sin bloquear el programa
// ============================================================
void loop() {
  unsigned long now = millis();

  leerBotones(now);  // Siempre: detectar pulsaciones

  // Cada 3 segundos: leer sensores y calcular ICA
  if (now - ts_sensor >= 3000) { leerSensores(); ts_sensor = now; }

  // Cada 150 ms: refrescar pantalla OLED
  if (now - ts_oled   >=  150) { mostrarOLED();  ts_oled   = now; }

  // Cada 3 segundos: enviar datos por Serial en formato JSON
  if (now - ts_serial >= 3000) { enviarJSON();   ts_serial = now; }

  // Siempre: actualizar color del LED RGB según ICA
  actualizarLED(now);
}


// ============================================================
//  leerBotones — Detecta pulsaciones con antirrebote por tiempo
//
//  Funcionamiento:
//    1. Al detectar flanco de subida (LOW→HIGH) se registra el momento.
//    2. Si el pin baja antes de cumplirse el tiempo mínimo → rebote, se ignora.
//    3. Si permanece HIGH el tiempo requerido → pulsación válida, se actúa.
//
//  Botón A: avanza a la pantalla siguiente (circular 0→1→2→3→0)
//  Botón B: retrocede a la pantalla anterior (circular 0→3→2→1→0)
//  Botón C: alterna entre modo MONITOREO y modo CONFIGURACIÓN
//           En modo CONFIG, el potenciómetro ajusta el umbral de temperatura
// ============================================================
void leerBotones(unsigned long now) {
  bool ra = digitalRead(PIN_A);
  bool rb = digitalRead(PIN_B);
  bool rc = digitalRead(PIN_C);

  // ── Botón A — pantalla siguiente ──────────────────────
  if (ra && !prev_A)  { estable_A = false; t_subio_A = now; } // Flanco subida: iniciar cuenta
  if (!ra)            { estable_A = false; }                   // Bajó antes: era rebote
  if (ra && !estable_A && (now - t_subio_A >= DB_AB) && (now - db_A >= DB_AB)) {
    estable_A = true;
    if (!cfg) pantalla = (pantalla + 1) % 4;  // Solo navega en modo MONITOR
    db_A = now;
  }
  prev_A = ra;

  // ── Botón B — pantalla anterior ───────────────────────
  if (rb && !prev_B)  { estable_B = false; t_subio_B = now; }
  if (!rb)            { estable_B = false; }
  if (rb && !estable_B && (now - t_subio_B >= DB_AB) && (now - db_B >= DB_AB)) {
    estable_B = true;
    if (!cfg) pantalla = (pantalla + 3) % 4;  // +3 equivale a -1 en módulo 4
    db_B = now;
  }
  prev_B = rb;

  // ── Botón C — toggle CONFIG/MONITOR ──────────────────
  // Usa 250 ms de confirmación para evitar cambios accidentales
  if (rc && !prev_C)  { estable_C = false; t_subio_C = now; }
  if (!rc)            { estable_C = false; }
  if (rc && !estable_C && (now - t_subio_C >= DB_C) && (now - db_C >= DB_C)) {
    estable_C = true;
    cfg = !cfg;
    tone(PIN_BUZ, cfg ? 600 : 1200, 80);  // Tono bajo=entra CONFIG, alto=sale CONFIG
    db_C = now;
  }
  prev_C = rc;

  // En modo CONFIG el potenciómetro ajusta el umbral de temperatura (20–40 °C)
  if (cfg) umbral = map(analogRead(PIN_POT), 0, 1023, 20, 40);
}


// ============================================================
//  leerSensores — Lee los tres sensores cada 3 segundos
//
//  - DHT22  : temperatura y humedad relativa
//  - BMP280 : presión barométrica (Pa) y temperatura
//             Lectura directa con readPressure() y readTemperature()
//  - POT    : CO2 simulado 400–2000 ppm (solo en modo MONITOR)
//
//  Solo actualiza las variables globales si la lectura es válida.
//  Al final recalcula el ICA con los nuevos datos.
// ============================================================
void leerSensores() {
  ica_prev = ica;  // Guardar ICA anterior para calcular tendencia

  // DHT22: temperatura y humedad
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) t_dht = t;  // Solo actualizar si la lectura es válida
  if (!isnan(h)) hum   = h;

  // BMP280: lectura directa (sin sensors_event_t, a diferencia del BMP180)
  float p  = bmp.readPressure();     // Retorna Pa directamente
  float tb = bmp.readTemperature();  // Retorna °C
  if (p > 0) {
    pres  = p;   // Guardamos en Pa (se convierte a hPa al mostrar)
    t_bmp = tb;
  }

  // Potenciómetro → CO2 simulado (solo cuando NO estamos en CONFIG,
  // porque en CONFIG el pot ya está siendo usado para el umbral)
  if (!cfg) co2 = map(analogRead(PIN_POT), 0, 1023, 400, 2000);

  // Recalcular ICA con los datos actualizados
  ica = calcICA();
}


// ============================================================
//  calcICA — Calcula el Índice de Calidad Ambiental (0–100)
//
//  Fórmula ponderada basada en tres variables normalizadas:
//    ICA = (0.4 × temp_norm + 0.3 × hum_norm + 0.3 × co2_norm) × 100
//
//  Normalización de cada variable (resultado entre 0.0 y 1.0):
//    temp_norm = 1 - constrain(|temp - 21| / 10, 0, 1)
//      → Óptimo en 21 °C. Penaliza si se aleja más de 10 °C.
//    hum_norm  = 1 - constrain(|hum - 50| / 30, 0, 1)
//      → Óptimo en 50%. Penaliza si se aleja más de 30%.
//    co2_norm  = 1 - constrain((co2 - 400) / 1600, 0, 1)
//      → Óptimo en 400 ppm. Penaliza linealmente hasta 2000 ppm.
//
//  Ejemplo numérico con temp=22°C, hum=55%, co2=500 ppm:
//    temp_norm = 1 - constrain(|22-21|/10, 0,1) = 1 - 0.10 = 0.90
//    hum_norm  = 1 - constrain(|55-50|/30, 0,1) = 1 - 0.17 = 0.83
//    co2_norm  = 1 - constrain((500-400)/1600, 0,1) = 1 - 0.06 = 0.94
//    ICA = (0.4×0.90 + 0.3×0.83 + 0.3×0.94) × 100
//        = (0.360  +  0.249  +  0.282) × 100
//        =  0.891 × 100 = 89.1  → Estado: ÓPTIMO
//
//  Niveles de ICA:
//    80–100 → ÓPTIMO    (LED verde)
//    50–79  → ACEPTABLE (LED azul)
//    20–49  → DEFICIENTE(LED amarillo)
//     0–19  → PELIGROSO (LED rojo + buzzer intermitente)
// ============================================================
float calcICA() {
  float tn = 1.0 - constrain(abs(t_dht - 21.0) / 10.0,   0.0, 1.0);
  float hn = 1.0 - constrain(abs(hum   - 50.0) / 30.0,   0.0, 1.0);
  float cn = 1.0 - constrain((co2 - 400.0)     / 1600.0, 0.0, 1.0);
  return (0.4*tn + 0.3*hn + 0.3*cn) * 100.0;
}


// ============================================================
//  tend — Calcula la tendencia del ICA respecto a la medición anterior
//
//  Compara el ICA actual con el anterior (ica_prev).
//  Un cambio menor a ±1.5 puntos se considera estable
//  para evitar falsos cambios de tendencia por ruido del sensor.
//
//  Retorna:
//    "SUBE"   → ICA mejoró más de 1.5 puntos
//    "BAJA"   → ICA empeoró más de 1.5 puntos
//    "ESTABLE"→ cambio dentro del margen de ±1.5 puntos
// ============================================================
const char* tend() {
  if (ica > ica_prev + 1.5) return "SUBE";
  if (ica < ica_prev - 1.5) return "BAJA";
  return "ESTABLE";
}


// ============================================================
//  oledRow — Helper: imprime etiqueta + valor float en el OLED
//
//  Parámetros:
//    y   : posición vertical en píxeles
//    lbl : etiqueta en Flash (F("..."))
//    val : valor float a mostrar
//    dec : decimales a mostrar
//    unit: unidad en Flash (F("..."))
// ============================================================
void oledRow(uint8_t y, const __FlashStringHelper* lbl, float val, uint8_t dec, const __FlashStringHelper* unit) {
  oled.setCursor(0, y);
  oled.print(lbl);
  oled.print(val, dec);
  oled.print(unit);
}


// ============================================================
//  oledRowS — Helper: imprime etiqueta + cadena de texto en el OLED
//
//  Parámetros:
//    y   : posición vertical en píxeles
//    lbl : etiqueta en Flash (F("..."))
//    val : cadena char* a mostrar
// ============================================================
void oledRowS(uint8_t y, const __FlashStringHelper* lbl, const char* val) {
  oled.setCursor(0, y);
  oled.print(lbl);
  oled.print(val);
}


// ============================================================
//  estadoICA — Retorna el estado textual del ICA actual
//
//  80–100 → "OPTIMO"
//  50–79  → "ACEPT."
//  20–49  → "DEFIC."
//   0–19  → "PELIG."
// ============================================================
const __FlashStringHelper* estadoICA() {
  if (ica >= 80) return F("OPTIMO");
  if (ica >= 50) return F("ACEPT.");
  if (ica >= 20) return F("DEFIC.");
  return F("PELIG.");
}


// ============================================================
//  mostrarOLED — Refresca la pantalla OLED cada 150 ms
//
//  Cabecera: modo actual ([MONITOR] o [CONFIG]) + número de pantalla
//  Línea separadora horizontal
//
//  Modo CONFIGURACIÓN:
//    Muestra el umbral de temperatura actual ajustable con el potenciómetro
//
//  Modo MONITOREO (4 pantallas navegables con botones A y B):
//    Pantalla 0 — ICA actual en grande + estado + tendencia
//    Pantalla 1 — Temperaturas DHT22 y BMP280 + umbral configurable
//    Pantalla 2 — Humedad, presión en hPa y altitud estimada
//    Pantalla 3 — ICA numérico + CO2 en ppm + estado + tendencia
// ============================================================
void mostrarOLED() {
  oled.clearDisplay();

  // ── Cabecera ──
  oled.setCursor(0, 0);
  oled.print(cfg ? F("[CONFIG]") : F("[MONITOR]"));
  if (!cfg) {
    oled.setCursor(104, 0);
    oled.print(pantalla + 1);
    oled.print(F("/4"));
  }
  oled.drawLine(0, 9, 127, 9, WHITE);

  if (cfg) {
    // ── Modo CONFIGURACIÓN ──────────────────────────────
    oled.setCursor(0, 12);  oled.print(F("Umbral temperatura"));
    oled.setTextSize(2);
    oled.setCursor(24, 26); oled.print(umbral, 1); oled.print(F("C"));
    oled.setTextSize(1);
    oled.setCursor(0, 50);  oled.print(F("Pot: ajusta umbral"));

  } else {
    // ── Modo MONITOREO ──────────────────────────────────
    switch (pantalla) {

      case 0:  // ICA principal
        oled.setCursor(0, 12); oled.print(F("ICA:"));
        oled.setTextSize(2);
        oled.setCursor(28, 22); oled.print(ica, 1);
        oled.setTextSize(1);
        oled.setCursor(0, 42); oled.print(estadoICA());
        oledRowS(52, F("Tend: "), tend());
        break;

      case 1:  // Temperatura
        oled.setCursor(0, 12); oled.print(F("-- TEMPERATURA --"));
        oledRow(24, F("DHT22: "), t_dht,  1, F(" C"));
        oledRow(34, F("BMP280:"), t_bmp,  1, F(" C"));
        oledRow(44, F("Umbral:"), umbral, 0, F(" C"));
        if (t_dht > umbral) { oled.setCursor(120, 44); oled.print(F("!")); }
        break;

      case 2: {  // Humedad + Presión + Altitud
        oled.setCursor(0, 12); oled.print(F("- HUM + PRESION -"));
        oledRow(24, F("Hum: "),  hum,         1, F(" %"));
        oledRow(34, F("Pres:"), pres / 100.0, 1, F(" hPa"));
        // Fórmula barométrica estándar para estimar altitud
        float alt = 44330.0 * (1.0 - pow((pres / 100.0) / 1013.25, 0.1903));
        oledRow(44, F("Alt: "),  alt,          0, F(" m"));
        break;
      }

      case 3:  // ICA + CO2
        oled.setCursor(0, 12); oled.print(F("-- ICA + CO2 --"));
        oledRow(24, F("ICA: "), ica, 1, F(""));
        oledRow(34, F("CO2: "), co2, 0, F(" ppm"));
        oled.setCursor(0, 44);
        oled.print(F("Est: ")); oled.print(estadoICA());
        oledRowS(54, F("Tend: "), tend());
        break;
    }
  }

  oled.display();
}


// ============================================================
//  actualizarLED — Controla el color del LED RGB según el ICA
//
//  El LED cambia de color para indicar visualmente el nivel
//  de calidad ambiental en tiempo real:
//
//    ICA 80–100 → Verde       (ÓPTIMO)
//    ICA 50–79  → Azul        (ACEPTABLE)
//    ICA 20–49  → Amarillo    (DEFICIENTE — R+G mezclados)
//    ICA  0–19  → Rojo fijo   (PELIGROSO)
//               + Buzzer intermitente cada 600 ms
//
//  Usa analogWrite() para PWM en los tres canales del LED RGB.
//  El buzzer en nivel PELIGROSO alterna encendido/apagado
//  sin usar delay(), controlado por ts_buz.
// ============================================================
void actualizarLED(unsigned long now) {
  // Apagar los tres canales antes de establecer el nuevo color
  analogWrite(PIN_R,     0);
  analogWrite(PIN_G,     0);
  analogWrite(PIN_B_LED, 0);

  if (ica >= 80) {
    // ÓPTIMO → Verde
    analogWrite(PIN_G, 200);

  } else if (ica >= 50) {
    // ACEPTABLE → Azul
    analogWrite(PIN_B_LED, 200);

  } else if (ica >= 20) {
    // DEFICIENTE → Amarillo (rojo + verde)
    analogWrite(PIN_R, 180);
    analogWrite(PIN_G,  80);

  } else {
    // PELIGROSO → Rojo + buzzer intermitente
    analogWrite(PIN_R, 255);
    if (now - ts_buz >= 600) {
      buz_on = !buz_on;
      if (buz_on) tone(PIN_BUZ, 880, 280);
      ts_buz = now;
    }
  }
}


// ============================================================
//  enviarJSON — Envía todas las lecturas por Monitor Serial
//
//  Formato JSON (REQ-07):
//  {"t":22.5,"h":55.0,"p":1013.2,"co2":500,"ICA":89.1,
//   "estado":"OPTIMO","tend":"ESTABLE"}
//
//  Campos:
//    t      → temperatura DHT22 (°C)
//    h      → humedad relativa (%)
//    p      → presión BMP280 (hPa)
//    co2    → CO2 simulado (ppm)
//    ICA    → Índice de Calidad Ambiental (0–100)
//    estado → nivel textual del ICA
//    tend   → tendencia respecto a la medición anterior
//
//  Se usa F("...") para guardar strings en Flash y ahorrar RAM.
// ============================================================
void enviarJSON() {
  Serial.print(F("{\"t\":"));         Serial.print(t_dht, 1);
  Serial.print(F(",\"h\":"));         Serial.print(hum,   1);
  Serial.print(F(",\"p\":"));         Serial.print(pres / 100.0, 1);
  Serial.print(F(",\"co2\":"));       Serial.print(co2,   0);
  Serial.print(F(",\"ICA\":"));       Serial.print(ica,   1);
  Serial.print(F(",\"estado\":\""));  Serial.print(estadoICA());
  Serial.print(F("\",\"tend\":\""));  Serial.print(tend());
  Serial.println(F("\"}"));
}
