// enlace.h — Comunicacion UART con la Raspberry Pi (Formato A ampliado)
//
// Responsabilidades:
//   - Armar y enviar tramas con checksum (PIEZA, ESTADO, LATIDO, ALERTA, CONTENEDORES).
//   - Recibir y validar tramas de la Pi y entregar los comandos al programa principal.
//   - Latido cada LATIDO_MS; el enlace se da por caido sin tramas validas en ENLACE_TIMEOUT_MS.
//   - Cola de eventos sin ACK: reintento cada REINTENTO_MS mientras haya enlace.
//     La cola se guarda en NVS (memoria.h) para sobrevivir a reinicios.
//
// La especificacion de las tramas esta en docs/protocolo-uart.md.

#pragma once
#include <Arduino.h>

#include "config.h"
#include "pendientes.h"
#include "protocolo.h"

class Enlace {
 public:
  // Se llama con cada comando valido recibido que no sea ACK/VIVO/REENVIAR
  // (esos los resuelve el propio Enlace).
  typedef void (*Manejador)(const proto::Comando& c);
  // Se llama cuando la cola de pendientes cambia (para guardarla en NVS).
  typedef void (*AlCambiarCola)();

  void begin(Manejador m, AlCambiarCola alCambiar);
  void actualizar(uint32_t versionTabla, uint32_t ultimoSeq);   // en cada vuelta de loop()

  bool activo() const { return activo_; }
  ColaPendientes& cola() { return cola_; }
  uint32_t tramasInvalidas() const { return invalidas_; }
  bool acabaDeConectar();   // true una vez cada vez que el enlace pasa de caido a activo

  void enviar(const char* cuerpo);
  void alerta(const char* codigo, const char* detalle = nullptr);
  void estado(const char* nombre);
  void contenedores(const uint16_t contenido[3], const uint16_t capacidad[3]);
  // Registra un evento PIEZA en la cola y lo envia si hay enlace.
  void pieza(uint32_t seq, const char* uid, char categoria, uint8_t motivo, uint32_t ciclo_ms);

  bool verRutinarias = false;   // mostrar LATIDO/ESTADO/VIVO en el Monitor Serial

 private:
  void leer();
  void procesar(char* linea);
  void enviarEvento(Evento& e);

  ColaPendientes cola_;
  Manejador manejador_ = nullptr;
  AlCambiarCola alCambiar_ = nullptr;
  char rx_[MAX_LINEA];
  size_t rxLen_ = 0;
  bool rxDesbordado_ = false;
  unsigned long ultimoRx_ = 0;
  unsigned long ultimoLatido_ = 0;
  bool activo_ = false;
  bool nuevoActivo_ = false;
  uint32_t invalidas_ = 0;
};
