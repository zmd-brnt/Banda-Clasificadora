"""Logica del enlace con el ESP32 vista desde la Raspberry Pi.

- Cada PIEZA se guarda una sola vez (clave epoca + seq) y siempre se confirma con ACK,
  aunque sea duplicada (asi el ESP32 deja de reenviarla).
- Si llega un seq mayor al esperado, se pide REENVIAR de los que faltan.
- Cada LATIDO se responde con VIVO; si la version de la tabla del ESP32 no coincide con la
  de la base de datos, se le envia SYNC (como mucho una vez cada SYNC_MIN_S).
- Si el LATIDO trae un ultimo_seq menor que el mayor seq registrado, el ESP32 perdio su
  memoria: se abre una epoca nueva para no descartar sus piezas como duplicadas.
- Sin tramas validas durante ENLACE_TIMEOUT_S el enlace se marca como caido.

Todo lo que pasa se publica como eventos (dicts) para el panel web (SSE).
"""

import logging
import queue
import threading
import time

from . import protocolo
from .db import ahora_ms

log = logging.getLogger("banda.supervisor")

ENLACE_TIMEOUT_S = 3.0
SYNC_MIN_S = 2.0
MAX_REENVIOS_POR_HUECO = 20


class Supervisor:
    def __init__(self, db, escribir):
        """escribir(bytes): envia una trama al ESP32 (puerto serie o equivalente)."""
        self.db = db
        self._escribir = escribir
        self._lock = threading.RLock()
        self._suscriptores = set()

        self.enlace_ok = False
        self.ultimo_rx = 0.0
        self.estado_esp32 = None
        self.version_esp32 = None
        self.pendientes_esp32 = 0
        self._ultimo_sync = 0.0
        self._solicitados = set()   # seq pedidos con REENVIAR que aun no llegan
        self.stats = {"tramas": 0, "invalidas": 0, "piezas": 0, "duplicados": 0, "reenvios_pedidos": 0}

    # ------------------------------------------------------------------ eventos para el panel
    def suscribir(self):
        q = queue.Queue(maxsize=1000)
        with self._lock:
            self._suscriptores.add(q)
        return q

    def desuscribir(self, q):
        with self._lock:
            self._suscriptores.discard(q)

    def publicar(self, evento):
        evento.setdefault("ts", ahora_ms())
        with self._lock:
            subs = list(self._suscriptores)
        for q in subs:
            try:
                q.put_nowait(evento)
            except queue.Full:   # cliente que no lee: se descarta el evento para el
                pass

    def alerta(self, nivel, codigo, mensaje):
        a = self.db.guardar_alerta(nivel, codigo, mensaje)
        self.publicar({"tipo": "ALERTA", **a})
        log.info("ALERTA %s %s: %s", nivel, codigo, mensaje)

    # ------------------------------------------------------------------ envio
    def enviar(self, cuerpo):
        try:
            self._escribir(protocolo.trama(cuerpo))
        except Exception as e:   # puerto cerrado o desconectado
            log.warning("No se pudo enviar %s: %s", cuerpo, e)
            return False
        if not cuerpo.startswith(("VIVO,", "ACK,")):
            log.info("[UART >] %s", cuerpo)
        return True

    # ------------------------------------------------------------------ recepcion
    def procesar_linea(self, linea, ahora=None):
        """Procesa una linea recibida del ESP32 (sin importar si trae '\\n')."""
        ahora = time.monotonic() if ahora is None else ahora
        linea = linea.strip()
        if not linea:
            return
        cuerpo = protocolo.validar(linea)
        if cuerpo is None:
            self.stats["invalidas"] += 1
            log.warning("[UART <] Trama invalida descartada: %r", linea)
            return
        try:
            msg = protocolo.interpretar(cuerpo)
        except protocolo.ErrorProtocolo as e:
            self.stats["invalidas"] += 1
            log.warning("[UART <] %s: %r", e, cuerpo)
            return

        with self._lock:
            self.stats["tramas"] += 1
            self.ultimo_rx = ahora
            if not self.enlace_ok:
                self.enlace_ok = True
                self.alerta("info", "ENLACE_OK", "Se recibe al ESP32 por UART.")
                self.publicar({"tipo": "ENLACE", "ok": True})

            manejador = {
                "PIEZA": self._pieza,
                "LATIDO": self._latido,
                "ESTADO": self._estado,
                "ALERTA": self._alerta_esp32,
                "CONTENEDORES": self._contenedores,
            }[msg["tipo"]]
            manejador(msg, ahora)

    def revisar_enlace(self, ahora=None):
        ahora = time.monotonic() if ahora is None else ahora
        with self._lock:
            if self.enlace_ok and ahora - self.ultimo_rx > ENLACE_TIMEOUT_S:
                self.enlace_ok = False
                self.alerta("critico", "ENLACE_CAIDO",
                            "Sin tramas del ESP32. Sigue clasificando con su tabla local; "
                            "los eventos llegaran al reconectar.")
                self.publicar({"tipo": "ENLACE", "ok": False})

    # ------------------------------------------------------------------ manejadores
    def _pieza(self, p, ahora):
        epoca = self.db.epoca()
        seq = p["seq"]
        if self.db.existe_pieza(epoca, seq):
            self.stats["duplicados"] += 1
            log.info("PIEZA seq %d duplicada: se descarta (y se confirma)", seq)
            self.enviar(protocolo.cmd_ack(seq))
            return

        maximo = self.db.max_seq(epoca)
        # Con el historial vacio no se piden huecos: el ESP32 pudo numerar piezas antes de que
        # existiera esta base de datos. (Los eventos sin ACK los reintenta el ESP32 de todos modos.)
        if maximo > 0 and seq > maximo + 1:
            faltan = [s for s in range(maximo + 1, seq) if s not in self._solicitados]
            faltan = faltan[:MAX_REENVIOS_POR_HUECO]
            if faltan:
                self.alerta("advertencia", "UART_HUECO",
                            f"Faltan eventos seq {', '.join(map(str, faltan))}; se pidio su reenvio.")
                for s in faltan:
                    self._solicitados.add(s)
                    self.stats["reenvios_pedidos"] += 1
                    self.enviar(protocolo.cmd_reenviar(s))

        tardio = p["reenvio"] or seq in self._solicitados or seq < maximo
        self._solicitados.discard(seq)
        ts = ahora_ms()
        self.db.guardar_pieza(epoca, p, ts, tardio)
        self.stats["piezas"] += 1
        self.enviar(protocolo.cmd_ack(seq))
        log.info("PIEZA seq %d  %s -> %s %s  ciclo %d ms%s", seq, p["uid"] or "-", p["categoria"],
                 p["motivo"] or "OK", p["ciclo_ms"], " (tardia)" if tardio else "")
        self.publicar({"tipo": "PIEZA", "epoca": epoca, "seq": seq, "ts": ts, "uid": p["uid"],
                       "categoria": p["categoria"], "motivo": p["motivo"], "ciclo_ms": p["ciclo_ms"],
                       "reenvio": p["reenvio"], "tardio": tardio, "nombre": self.db.nombre_de(p["uid"])})

    def _latido(self, m, ahora):
        self.pendientes_esp32 = m["pendientes"]
        self.version_esp32 = m["version_tabla"]
        version = self.db.version_tabla()
        self.enviar(protocolo.cmd_vivo(version))

        if m["ultimo_seq"] is not None:
            epoca = self.db.epoca()
            if m["ultimo_seq"] < self.db.max_seq(epoca):
                nueva = self.db.nueva_epoca()
                self._solicitados.clear()
                self.alerta("advertencia", "NUMERACION_REINICIADA",
                            f"El ESP32 reporta ultimo seq {m['ultimo_seq']} pero ya se registraron mayores: "
                            f"perdio su memoria. Se inicia la epoca {nueva} del historial.")

        if self.version_esp32 != version and ahora - self._ultimo_sync >= SYNC_MIN_S:
            self.sincronizar(ahora)

        self.publicar({"tipo": "LATIDO", "pendientes": m["pendientes"], "version_tabla": m["version_tabla"]})

    def _estado(self, m, ahora):
        self.estado_esp32 = m["estado"]
        self.publicar({"tipo": "ESTADO", "estado": m["estado"], "detalle": ""})

    def _alerta_esp32(self, m, ahora):
        codigo, detalle = m["codigo"], m["detalle"]
        if codigo == "SEQ_NO_DISPONIBLE" and detalle and detalle.isdigit():
            self._solicitados.discard(int(detalle))
        nivel, mensaje = protocolo.describir_alerta(codigo, detalle)
        self.alerta(nivel, codigo, mensaje)

    def _contenedores(self, m, ahora):
        self.db.actualizar_contenedores(m["contenido"], m["capacidad"])
        self.publicar({"tipo": "CONTENEDORES", "contenedores": self.db.contenedores()})

    # ------------------------------------------------------------------ comandos del panel
    def sincronizar(self, ahora=None):
        self._ultimo_sync = time.monotonic() if ahora is None else ahora
        version = self.db.version_tabla()
        cuerpo = protocolo.cmd_sync(version, self.db.tabla_sync())
        log.info("SYNC v%d (%d etiquetas) -> ESP32 (tenia v%s)", version, len(self.db.tabla_sync()),
                 self.version_esp32)
        return self.enviar(cuerpo)

    def comando(self, cmd):
        """Comando que llega del panel: dict con 'cmd'. Devuelve (ok, mensaje)."""
        tipo = cmd.get("cmd")
        try:
            if tipo == "SYNC":
                ok = self.sincronizar()
            elif tipo == "VACIADO":
                ok = self.enviar(protocolo.cmd_vaciado(cmd.get("categoria")))
            elif tipo == "CAPACIDAD":
                ok = self.enviar(protocolo.cmd_capacidad(cmd.get("categoria"), cmd.get("capacidad")))
            elif tipo == "PARO":
                ok = self.enviar(protocolo.cmd_paro())
                self.alerta("critico", "PARO_PANEL", "Se pidio un paro de emergencia desde el panel web.")
            else:
                return False, f"comando desconocido: {tipo!r}"
        except (protocolo.ErrorProtocolo, TypeError, ValueError) as e:
            return False, str(e)
        if not ok:
            return False, "no se pudo escribir en el puerto serie"
        if not self.enlace_ok:
            return True, "enviado, pero el enlace con el ESP32 esta caido: puede no llegar"
        return True, "enviado"

    def etiquetas_cambiaron(self):
        self.publicar({"tipo": "ETIQUETAS", "version": self.db.version_tabla(), "etiquetas": self.db.etiquetas()})

    # ------------------------------------------------------------------ resumen para el panel
    def instantanea(self):
        with self._lock:
            return {
                "modo": "uart",
                "enlace_ok": self.enlace_ok,
                "estado": self.estado_esp32,
                "version_tabla": self.db.version_tabla(),
                "version_esp32": self.version_esp32,
                "pendientes": self.pendientes_esp32,
                "etiquetas": self.db.etiquetas(),
                "piezas": self.db.piezas(),
                "alertas": self.db.alertas(),
                "contenedores": self.db.contenedores(),
                "stats": dict(self.stats),
            }
