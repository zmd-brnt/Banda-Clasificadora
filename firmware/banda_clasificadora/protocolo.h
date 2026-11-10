// protocolo.h — Tramas UART entre el ESP32 y la Raspberry Pi (Formato A ampliado)
//
// Logica pura: no usa Arduino, por eso tambien se compila y prueba en la PC
// (firmware/test/). La especificacion completa esta en docs/protocolo-uart.md.
//
// Formato de linea:  <cuerpo>*HH\n   HH = XOR de todos los caracteres del cuerpo, en hexadecimal.

#pragma once
#include <stddef.h>
#include <stdint.h>

namespace proto {

// XOR de los caracteres de s.
uint8_t checksum(const char* s);

// Escribe "<cuerpo>*HH\n" en destino. Devuelve la longitud escrita, o 0 si no cabe.
size_t armarTrama(const char* cuerpo, char* destino, size_t tam);

// Valida "<cuerpo>*HH" (sin salto de linea). Si es valida, corta la linea en el '*'
// (linea queda solo con el cuerpo) y devuelve true. Si no, no la modifica y devuelve false.
bool validarTrama(char* linea);

// true si s es un numero decimal sin signo, no vacio, que cabe en 32 bits.
bool esNumero(const char* s, size_t largo);

enum TipoComando {
  CMD_DESCONOCIDO,
  CMD_ACK,        // ACK,<seq>
  CMD_VIVO,       // VIVO,<version_tabla>
  CMD_SYNC,       // SYNC,<version>,<uid>=<cat>;...     (datos apunta a "<version>,...")
  CMD_REENVIAR,   // REENVIAR,<seq>
  CMD_VACIADO,    // VACIADO,<A|B|R>
  CMD_CAPACIDAD,  // CAPACIDAD,<A|B|R>,<n>   1 <= n <= 999
  CMD_PARO,       // PARO   (paro por software pedido desde el panel)
};

struct Comando {
  TipoComando tipo;
  uint32_t numero;     // seq (ACK, REENVIAR), version (VIVO) o capacidad (CAPACIDAD)
  char categoria;      // VACIADO, CAPACIDAD
  const char* datos;   // SYNC: apunta dentro del cuerpo recibido
};

// Interpreta un cuerpo ya validado. Devuelve false si el formato no corresponde a
// ningun comando valido (c.tipo queda en CMD_DESCONOCIDO).
bool interpretarComando(const char* cuerpo, Comando& c);

}  // namespace proto
