#include "pendientes.h"

#include <string.h>

const char* nombreMotivo(uint8_t m) {
  switch (m) {
    case MOTIVO_OK: return "OK";
    case MOTIVO_SIN_LECTURA: return "SIN_LECTURA";
    case MOTIVO_NO_REGISTRADA: return "NO_REGISTRADA";
    default: return "OK";
  }
}

void ColaPendientes::vaciar() { memset(ev_, 0, sizeof(ev_)); }

Evento& ColaPendientes::agregar(uint32_t seq, const char* uid, char categoria, uint8_t motivo,
                                uint32_t ciclo_ms, uint32_t* perdido) {
  if (perdido) *perdido = 0;

  int libre = -1;
  int masAntiguo = 0;
  for (int i = 0; i < MAX_PENDIENTES; i++) {
    if (!ev_[i].usado) {
      libre = i;
      break;
    }
    if (ev_[i].seq < ev_[masAntiguo].seq) masAntiguo = i;
  }
  if (libre < 0) {
    if (perdido) *perdido = ev_[masAntiguo].seq;
    libre = masAntiguo;
  }

  Evento& e = ev_[libre];
  memset(&e, 0, sizeof(e));
  e.usado = 1;
  e.seq = seq;
  strncpy(e.uid, uid, MAX_UID);
  e.uid[MAX_UID] = '\0';
  e.categoria = categoria;
  e.motivo = motivo;
  e.ciclo_ms = ciclo_ms;
  return e;
}

Evento* ColaPendientes::buscar(uint32_t seq) {
  for (int i = 0; i < MAX_PENDIENTES; i++) {
    if (ev_[i].usado && ev_[i].seq == seq) return &ev_[i];
  }
  return nullptr;
}

bool ColaPendientes::quitar(uint32_t seq) {
  Evento* e = buscar(seq);
  if (!e) return false;
  e->usado = 0;
  return true;
}

int ColaPendientes::cantidad() const {
  int n = 0;
  for (int i = 0; i < MAX_PENDIENTES; i++) {
    if (ev_[i].usado) n++;
  }
  return n;
}
