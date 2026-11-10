#include "tabla.h"

#include <stdlib.h>
#include <string.h>

#include "protocolo.h"

static bool uidValido(const char* uid, size_t largo) {
  if (largo == 0 || largo > MAX_UID) return false;
  for (size_t i = 0; i < largo; i++) {
    char c = uid[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}

bool TablaLocal::cargar(const char* datos) {
  const char* coma = strchr(datos, ',');
  if (!coma || !proto::esNumero(datos, (size_t)(coma - datos))) return false;
  uint32_t version = (uint32_t)strtoul(datos, nullptr, 10);

  Etiqueta nueva[MAX_ETIQUETAS];
  int n = 0;
  const char* p = coma + 1;

  while (*p) {
    const char* fin = strchr(p, ';');
    size_t largo = fin ? (size_t)(fin - p) : strlen(p);

    if (largo > 0) {   // tolera ";;" y un ';' final
      const char* igual = (const char*)memchr(p, '=', largo);
      if (!igual) return false;
      size_t largoUid = (size_t)(igual - p);
      size_t largoCat = largo - largoUid - 1;
      char cat = igual[1];
      if (!uidValido(p, largoUid)) return false;
      if (largoCat != 1 || (cat != 'A' && cat != 'B' && cat != 'R')) return false;
      if (n >= MAX_ETIQUETAS) return false;

      for (int i = 0; i < n; i++) {   // UID repetido = tabla mal formada
        if (strlen(nueva[i].uid) == largoUid && strncmp(nueva[i].uid, p, largoUid) == 0) return false;
      }
      memcpy(nueva[n].uid, p, largoUid);
      nueva[n].uid[largoUid] = '\0';
      nueva[n].categoria = cat;
      n++;
    }
    if (!fin) break;
    p = fin + 1;
  }

  memcpy(e_, nueva, sizeof(Etiqueta) * (size_t)n);
  n_ = n;
  version_ = version;
  return true;
}

char TablaLocal::buscar(const char* uid) const {
  for (int i = 0; i < n_; i++) {
    if (strcmp(e_[i].uid, uid) == 0) return e_[i].categoria;
  }
  return '\0';
}
