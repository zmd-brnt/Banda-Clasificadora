#include "enlace.h"

void Enlace::begin(Manejador m, AlCambiarCola alCambiar) {
  manejador_ = m;
  alCambiar_ = alCambiar;
  Serial2.setRxBufferSize(1024);
  Serial2.begin(UART_BAUD, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);
  // Los eventos recuperados de NVS se reenvian en cuanto haya enlace, marcados como reenvio
  for (int i = 0; i < MAX_PENDIENTES; i++) {
    Evento& e = cola_.en(i);
    e.ultimoEnvio = 0;
    if (e.usado && e.envios == 0) e.envios = 1;
  }
}

bool Enlace::acabaDeConectar() {
  bool r = nuevoActivo_;
  nuevoActivo_ = false;
  return r;
}

// ---------------------------------------------------------------- Envio

void Enlace::enviar(const char* cuerpo) {
  char trama[MAX_LINEA + 8];
  if (proto::armarTrama(cuerpo, trama, sizeof(trama)) == 0) return;
  Serial2.print(trama);

  bool rutinaria = strncmp(cuerpo, "LATIDO,", 7) == 0 || strncmp(cuerpo, "ESTADO,", 7) == 0;
  if (verRutinarias || !rutinaria) {
    Serial.print("  [UART >] ");
    Serial.print(trama);
  }
}

void Enlace::alerta(const char* codigo, const char* detalle) {
  char cuerpo[64];
  if (detalle) {
    snprintf(cuerpo, sizeof(cuerpo), "ALERTA,%s,%s", codigo, detalle);
  } else {
    snprintf(cuerpo, sizeof(cuerpo), "ALERTA,%s", codigo);
  }
  enviar(cuerpo);
}

void Enlace::estado(const char* nombre) {
  char cuerpo[40];
  snprintf(cuerpo, sizeof(cuerpo), "ESTADO,%s", nombre);
  enviar(cuerpo);
}

void Enlace::contenedores(const uint16_t contenido[3], const uint16_t capacidad[3]) {
  char cuerpo[64];
  snprintf(cuerpo, sizeof(cuerpo), "CONTENEDORES,%u,%u,%u,%u,%u,%u", contenido[0], contenido[1],
           contenido[2], capacidad[0], capacidad[1], capacidad[2]);
  enviar(cuerpo);
}

void Enlace::enviarEvento(Evento& e) {
  char cuerpo[96];
  snprintf(cuerpo, sizeof(cuerpo), "PIEZA,%lu,%s,%c,%s,%lu,%d", (unsigned long)e.seq, e.uid,
           e.categoria, nombreMotivo(e.motivo), (unsigned long)e.ciclo_ms, e.envios > 0 ? 1 : 0);
  enviar(cuerpo);
  e.ultimoEnvio = millis();
  if (e.envios < 255) e.envios++;
}

void Enlace::pieza(uint32_t seq, const char* uid, char categoria, uint8_t motivo, uint32_t ciclo_ms) {
  uint32_t perdido = 0;
  Evento& e = cola_.agregar(seq, uid, categoria, motivo, ciclo_ms, &perdido);
  if (perdido) {
    Serial.printf("  ADVERTENCIA: cola de pendientes llena, se pierde seq %lu\n", (unsigned long)perdido);
    char det[12];
    snprintf(det, sizeof(det), "%lu", (unsigned long)perdido);
    alerta("PENDIENTES_PERDIDOS", det);
  }
  if (activo_) {
    enviarEvento(e);
  } else {
    // Se marca como ya intentado: cuando por fin se envie, llegara con reenvio = 1
    // (la Pi y el panel lo muestran como "recibido tarde").
    e.ultimoEnvio = millis();
    e.envios = 1;
    Serial.println("  Sin enlace: el evento queda pendiente");
  }
  if (alCambiar_) alCambiar_();
}

// ---------------------------------------------------------------- Recepcion

void Enlace::procesar(char* linea) {
  if (!proto::validarTrama(linea)) {
    invalidas_++;
    Serial.print("  [UART <] Trama invalida, descartada: ");
    Serial.println(linea);
    return;
  }

  ultimoRx_ = millis();
  if (!activo_) {
    activo_ = true;
    nuevoActivo_ = true;
    Serial.println(">> Enlace con la Raspberry Pi ACTIVO");
  }

  proto::Comando c;
  if (!proto::interpretarComando(linea, c)) {
    invalidas_++;
    Serial.print("  [UART <] Comando desconocido o mal formado: ");
    Serial.println(linea);
    return;
  }

  switch (c.tipo) {
    case proto::CMD_ACK:
      if (cola_.quitar(c.numero)) {
        Serial.printf("  [ACK] seq %lu confirmado. Pendientes: %d\n", (unsigned long)c.numero, cola_.cantidad());
        if (alCambiar_) alCambiar_();
      }
      break;   // ACK de algo que ya no esta pendiente (duplicado): se ignora
    case proto::CMD_VIVO:
      if (verRutinarias) {
        Serial.print("  [UART <] ");
        Serial.println(linea);
      }
      break;
    case proto::CMD_REENVIAR: {
      Evento* e = cola_.buscar(c.numero);
      Serial.printf("  [UART <] La Pi pide reenviar seq %lu\n", (unsigned long)c.numero);
      if (e) {
        enviarEvento(*e);
      } else {
        // Ya confirmado o perdido: se avisa para que la Pi deje de esperarlo
        char det[12];
        snprintf(det, sizeof(det), "%lu", (unsigned long)c.numero);
        alerta("SEQ_NO_DISPONIBLE", det);
      }
      break;
    }
    default:
      Serial.print("  [UART <] ");
      Serial.println(linea);
      if (manejador_) manejador_(c);
  }
}

void Enlace::leer() {
  while (Serial2.available()) {
    char ch = (char)Serial2.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      rx_[rxLen_] = '\0';
      if (rxDesbordado_) {
        invalidas_++;
        Serial.println("  [UART <] Linea demasiado larga, descartada");
      } else if (rxLen_ > 0) {
        procesar(rx_);
      }
      rxLen_ = 0;
      rxDesbordado_ = false;
      continue;
    }
    if (rxLen_ < MAX_LINEA - 1) {
      rx_[rxLen_++] = ch;
    } else {
      rxDesbordado_ = true;
    }
  }
}

void Enlace::actualizar(uint32_t versionTabla, uint32_t ultimoSeq) {
  leer();

  if (activo_ && millis() - ultimoRx_ > ENLACE_TIMEOUT_MS) {
    activo_ = false;
    Serial.println(">> Enlace con la Raspberry Pi CAIDO: se sigue clasificando con la tabla local");
  }

  if (millis() - ultimoLatido_ >= LATIDO_MS) {
    ultimoLatido_ = millis();
    char cuerpo[64];
    snprintf(cuerpo, sizeof(cuerpo), "LATIDO,%d,%lu,%lu", cola_.cantidad(), (unsigned long)versionTabla,
             (unsigned long)ultimoSeq);
    enviar(cuerpo);
  }

  if (!activo_) return;   // sin enlace los pendientes se conservan
  for (int i = 0; i < MAX_PENDIENTES; i++) {
    Evento& e = cola_.en(i);
    if (e.usado && (e.envios == 0 || millis() - e.ultimoEnvio >= REINTENTO_MS)) {
      if (e.envios > 0) Serial.printf("  [REINTENTO] seq %lu (envio #%u)\n", (unsigned long)e.seq, e.envios + 1);
      enviarEvento(e);
    }
  }
}
