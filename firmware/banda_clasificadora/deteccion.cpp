#include "deteccion.h"

float distanciaCm(uint32_t ecoUs) { return (float)ecoUs * 0.0343f / 2.0f; }

bool FiltroPresencia::actualizar(bool cercano) {
  if (cercano == presente_) {   // la medicion confirma el estado actual
    cuenta_ = 0;
    return false;
  }
  cuenta_++;
  uint8_t necesarias = presente_ ? nLib_ : nAct_;
  if (cuenta_ >= necesarias) {
    presente_ = !presente_;
    cuenta_ = 0;
    return true;
  }
  return false;
}
