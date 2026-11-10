// nfc.h — Lectura del UID con el RC522
//
// Mismo procedimiento que pruebas/integracion_test: WUPA despierta etiquetas nuevas y las
// que quedaron en HALT, por eso una etiqueta que ya se leyo se puede volver a leer.

#pragma once
#include <Arduino.h>

#include "tabla.h"   // MAX_UID

class LectorNfc {
 public:
  bool begin();   // false si el RC522 no responde (version 0x00 o 0xFF)
  bool ok() const { return ok_; }
  uint8_t version() const { return version_; }
  // Intenta leer el UID una vez. destino: hexadecimal MAYUSCULAS sin ':' (MAX_UID + 1 bytes).
  bool leerUid(char* destino);

 private:
  bool ok_ = false;
  uint8_t version_ = 0;
};
