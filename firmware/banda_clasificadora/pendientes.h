// pendientes.h — Eventos PIEZA enviados (o por enviar) que aun no tienen ACK
//
// Logica pura (sin Arduino). La estructura es de tamano fijo y sin punteros para poder
// guardarla tal cual en NVS y recuperarla despues de un reinicio.

#pragma once
#include <stdint.h>

#include "tabla.h"   // MAX_UID

#define MAX_PENDIENTES 16

enum Motivo : uint8_t { MOTIVO_OK = 0, MOTIVO_SIN_LECTURA = 1, MOTIVO_NO_REGISTRADA = 2 };
const char* nombreMotivo(uint8_t m);

struct Evento {
  uint8_t usado;            // 0 / 1
  uint8_t envios;           // veces que se ha transmitido
  char categoria;           // 'A', 'B', 'R'
  uint8_t motivo;           // Motivo
  uint32_t seq;
  uint32_t ciclo_ms;
  uint32_t ultimoEnvio;     // millis() del ultimo envio (no se usa tras un reinicio)
  char uid[MAX_UID + 1];    // "-" si no se leyo
};

class ColaPendientes {
 public:
  ColaPendientes() { vaciar(); }

  void vaciar();

  // Agrega un evento. Si la cola esta llena, sustituye el de seq mas antiguo y deja su seq
  // en *perdido (0 si no se perdio nada).
  Evento& agregar(uint32_t seq, const char* uid, char categoria, uint8_t motivo,
                  uint32_t ciclo_ms, uint32_t* perdido);

  Evento* buscar(uint32_t seq);
  bool quitar(uint32_t seq);   // false si ese seq no estaba (ACK duplicado)
  int cantidad() const;

  Evento& en(int i) { return ev_[i]; }

  // Para guardar/recuperar en NVS.
  void* datos() { return ev_; }
  static constexpr unsigned tamDatos() { return sizeof(Evento) * MAX_PENDIENTES; }

 private:
  Evento ev_[MAX_PENDIENTES];
};
