// deteccion.h — Conversion de tiempo de eco a distancia y filtro de presencia (HC-SR04)
//
// Logica pura (sin Arduino): se prueba en la PC (firmware/test/).
//
// Por que un filtro: una sola medicion del HC-SR04 puede fallar (eco perdido, rebote en la
// estructura). Se exige que varias mediciones seguidas coincidan antes de cambiar de
// "libre" a "presente" y al reves (histeresis por conteo).

#pragma once
#include <stdint.h>

// Distancia en cm a partir del ancho del pulso Echo en microsegundos.
// Velocidad del sonido ~343 m/s a ~20 °C -> 0.0343 cm/us, ida y vuelta -> /2.
float distanciaCm(uint32_t ecoUs);

class FiltroPresencia {
 public:
  // nActivar: mediciones "cercano" seguidas para pasar a presente.
  // nLiberar: mediciones "lejano o sin eco" seguidas para pasar a libre.
  FiltroPresencia(uint8_t nActivar, uint8_t nLiberar)
      : nAct_(nActivar), nLib_(nLiberar), cuenta_(0), presente_(false) {}

  // Entrega una medicion nueva. Devuelve true si el estado (presente/libre) cambio.
  bool actualizar(bool cercano);

  bool presente() const { return presente_; }
  void reiniciar() { cuenta_ = 0; presente_ = false; }

 private:
  uint8_t nAct_, nLib_;
  uint8_t cuenta_;   // mediciones seguidas que contradicen el estado actual
  bool presente_;
};
