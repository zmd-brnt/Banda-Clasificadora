// ultrasonico.h — Dos HC-SR04 (inspeccion y salida) sin bloquear el programa
//
// Cada sensor se dispara por turnos (nunca los dos a la vez, para que uno no reciba el
// eco del otro). El ancho del pulso Echo se mide con interrupciones, asi loop() no se
// detiene esperando el eco como pasaria con pulseIn().
//
// Echo llega al ESP32 a traves de un divisor de voltaje (docs/conexiones.md §5.3).

#pragma once
#include <Arduino.h>

#include "deteccion.h"

enum SensorId { SENSOR_INSPECCION = 0, SENSOR_SALIDA = 1 };

class Ultrasonicos {
 public:
  void begin();
  void actualizar();   // llamar en cada vuelta de loop()

  bool presente(SensorId s) const { return filtro_[s].presente(); }
  // Ultima distancia medida en cm, o -1 si no hubo eco valido.
  float distancia(SensorId s) const { return ultima_[s]; }
  uint32_t mediciones(SensorId s) const { return cuenta_[s]; }

 private:
  void disparar(int s);
  void cerrarMedicion(int s);

  FiltroPresencia filtro_[2] = {FiltroPresencia(2, 3), FiltroPresencia(2, 3)};
  float ultima_[2] = {-1, -1};
  uint32_t cuenta_[2] = {0, 0};
  int actual_ = 0;
  unsigned long tVentana_ = 0;
  int descartes_ = 0;
  bool midiendo_ = false;
};
