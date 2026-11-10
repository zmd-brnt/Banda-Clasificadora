#include "ultrasonico.h"

#include "config.h"

static const uint8_t TRIG[2] = {PIN_TRIG_INSP, PIN_TRIG_SALIDA};
static const uint8_t ECHO[2] = {PIN_ECHO_INSP, PIN_ECHO_SALIDA};
static const float UMBRAL[2] = {UMBRAL_INSP_CM, UMBRAL_SALIDA_CM};

// Marcas de tiempo escritas por las interrupciones
static volatile uint32_t tSubida[2];
static volatile uint32_t tBajada[2];
static volatile bool subio[2];
static volatile bool bajo[2];

static void IRAM_ATTR atenderEcho(int s) {
  uint32_t ahora = micros();
  if (digitalRead(ECHO[s])) {
    tSubida[s] = ahora;
    subio[s] = true;
    bajo[s] = false;
  } else if (subio[s] && !bajo[s]) {
    tBajada[s] = ahora;
    bajo[s] = true;
  }
}
static void IRAM_ATTR isrEchoInspeccion() { atenderEcho(0); }
static void IRAM_ATTR isrEchoSalida() { atenderEcho(1); }

void Ultrasonicos::begin() {
  filtro_[0] = FiltroPresencia(MUESTRAS_ACTIVAR, MUESTRAS_LIBERAR);
  filtro_[1] = FiltroPresencia(MUESTRAS_ACTIVAR, MUESTRAS_LIBERAR);
  for (int s = 0; s < 2; s++) {
    pinMode(TRIG[s], OUTPUT);
    digitalWrite(TRIG[s], LOW);
    pinMode(ECHO[s], INPUT);   // GPIO34/35: solo entrada, sin pull-up interno
  }
  attachInterrupt(digitalPinToInterrupt(PIN_ECHO_INSP), isrEchoInspeccion, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ECHO_SALIDA), isrEchoSalida, CHANGE);
  descartes_ = MEDICIONES_DESCARTE;
  actual_ = 0;
  disparar(actual_);
}

void Ultrasonicos::disparar(int s) {
  noInterrupts();
  subio[s] = false;
  bajo[s] = false;
  interrupts();
  digitalWrite(TRIG[s], HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG[s], LOW);
  tVentana_ = millis();
  midiendo_ = true;
}

void Ultrasonicos::cerrarMedicion(int s) {
  noInterrupts();
  bool completo = subio[s] && bajo[s];
  uint32_t ancho = tBajada[s] - tSubida[s];
  interrupts();

  float cm = completo ? distanciaCm(ancho) : -1.0f;
  if (cm >= 0 && cm < DISTANCIA_MIN_CM) cm = -1.0f;   // eco demasiado corto: invalido
  ultima_[s] = cm;
  cuenta_[s]++;

  if (descartes_ > 0) {   // primeras mediciones tras encender: se ignoran
    descartes_--;
    return;
  }
  // Sin eco o mas lejos que el umbral = libre
  filtro_[s].actualizar(cm >= 0 && cm <= UMBRAL[s]);
}

void Ultrasonicos::actualizar() {
  if (!midiendo_ || millis() - tVentana_ < MEDICION_MS) return;
  cerrarMedicion(actual_);
  actual_ = 1 - actual_;
  disparar(actual_);
}
