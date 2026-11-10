// tabla.h — Tabla local UID -> categoria del ESP32
//
// Es la copia que permite clasificar sin la Raspberry Pi. Llega con SYNC y se guarda en NVS.
// Logica pura (sin Arduino): se prueba en la PC (firmware/test/).

#pragma once
#include <stdint.h>

#define MAX_ETIQUETAS 32
#define MAX_UID 20   // NTAG213: 7 bytes = 14 caracteres hex; margen para otros tipos

struct Etiqueta {
  char uid[MAX_UID + 1];
  char categoria;   // 'A', 'B' o 'R'
};

class TablaLocal {
 public:
  TablaLocal() : n_(0), version_(0) {}

  // datos = "<version>,<uid>=<cat>;<uid>=<cat>;..."  (la lista puede estar vacia).
  // UID en hexadecimal MAYUSCULAS sin ':'. Se valida todo antes de reemplazar la tabla:
  // si algo esta mal, se conserva la anterior y devuelve false.
  bool cargar(const char* datos);

  // Categoria del UID, o '\0' si no esta registrado.
  char buscar(const char* uid) const;

  int cantidad() const { return n_; }
  uint32_t version() const { return version_; }
  const Etiqueta& en(int i) const { return e_[i]; }

 private:
  Etiqueta e_[MAX_ETIQUETAS];
  int n_;
  uint32_t version_;
};
