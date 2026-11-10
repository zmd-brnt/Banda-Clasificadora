#include "protocolo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace proto {

uint8_t checksum(const char* s) {
  uint8_t x = 0;
  while (*s) {
    x ^= (uint8_t)*s++;
  }
  return x;
}

size_t armarTrama(const char* cuerpo, char* destino, size_t tam) {
  int n = snprintf(destino, tam, "%s*%02X\n", cuerpo, checksum(cuerpo));
  if (n < 0 || (size_t)n >= tam) {
    if (tam > 0) destino[0] = '\0';
    return 0;
  }
  return (size_t)n;
}

static int valorHex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

bool validarTrama(char* linea) {
  size_t largo = strlen(linea);
  if (largo < 4 || linea[largo - 3] != '*') return false;   // cuerpo de al menos 1 caracter
  int alto = valorHex(linea[largo - 2]);
  int bajo = valorHex(linea[largo - 1]);
  if (alto < 0 || bajo < 0) return false;

  linea[largo - 3] = '\0';
  if (checksum(linea) != (uint8_t)(alto * 16 + bajo)) {
    linea[largo - 3] = '*';   // se deja como estaba
    return false;
  }
  return true;
}

bool esNumero(const char* s, size_t largo) {
  if (largo == 0 || largo > 10) return false;
  uint64_t v = 0;
  for (size_t i = 0; i < largo; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    v = v * 10 + (uint64_t)(s[i] - '0');
  }
  return v <= 0xFFFFFFFFull;
}

static bool esCategoria(char c) { return c == 'A' || c == 'B' || c == 'R'; }

// Numero que ocupa todo el resto de la cadena.
static bool numeroFinal(const char* s, uint32_t& v) {
  size_t largo = strlen(s);
  if (!esNumero(s, largo)) return false;
  v = (uint32_t)strtoul(s, nullptr, 10);
  return true;
}

bool interpretarComando(const char* cuerpo, Comando& c) {
  c.tipo = CMD_DESCONOCIDO;
  c.numero = 0;
  c.categoria = '\0';
  c.datos = nullptr;

  if (strncmp(cuerpo, "ACK,", 4) == 0) {
    if (!numeroFinal(cuerpo + 4, c.numero)) return false;
    c.tipo = CMD_ACK;
  } else if (strncmp(cuerpo, "VIVO,", 5) == 0) {
    if (!numeroFinal(cuerpo + 5, c.numero)) return false;
    c.tipo = CMD_VIVO;
  } else if (strncmp(cuerpo, "REENVIAR,", 9) == 0) {
    if (!numeroFinal(cuerpo + 9, c.numero)) return false;
    c.tipo = CMD_REENVIAR;
  } else if (strncmp(cuerpo, "SYNC,", 5) == 0) {
    const char* datos = cuerpo + 5;
    const char* coma = strchr(datos, ',');
    if (!coma || !esNumero(datos, (size_t)(coma - datos))) return false;
    c.numero = (uint32_t)strtoul(datos, nullptr, 10);
    c.datos = datos;
    c.tipo = CMD_SYNC;
  } else if (strncmp(cuerpo, "VACIADO,", 8) == 0) {
    const char* r = cuerpo + 8;
    if (!esCategoria(r[0]) || r[1] != '\0') return false;
    c.categoria = r[0];
    c.tipo = CMD_VACIADO;
  } else if (strncmp(cuerpo, "CAPACIDAD,", 10) == 0) {
    const char* r = cuerpo + 10;
    if (!esCategoria(r[0]) || r[1] != ',') return false;
    if (!numeroFinal(r + 2, c.numero) || c.numero < 1 || c.numero > 999) return false;
    c.categoria = r[0];
    c.tipo = CMD_CAPACIDAD;
  } else if (strcmp(cuerpo, "PARO") == 0) {
    c.tipo = CMD_PARO;
  } else {
    return false;
  }
  return true;
}

}  // namespace proto
