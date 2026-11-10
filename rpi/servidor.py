#!/usr/bin/env python3
"""Supervisor de la banda clasificadora: lee la UART del ESP32, guarda en SQLite y sirve el panel web.

Uso en la Raspberry Pi:
    python3 servidor.py                       # /dev/serial0, panel en http://<ip-de-la-pi>:8080
    python3 servidor.py --puerto /dev/ttyUSB0 # otro puerto serie
    python3 servidor.py --sin-uart            # solo el panel y la base de datos (sin ESP32)

Para trabajar sin la placa, en otra terminal:
    python3 emulador_esp32.py                 # imprime un puerto /dev/pts/N
    python3 servidor.py --puerto /dev/pts/N

Requisitos: Python 3.9+ y pyserial (sudo apt install python3-serial). Ver rpi/README.md.
"""

import argparse
import logging
import os
import signal
import sys
import threading
import time

from banda.db import BaseDatos
from banda.servidor import crear_servidor
from banda.supervisor import Supervisor

AQUI = os.path.dirname(os.path.abspath(__file__))
log = logging.getLogger("banda")


class PuertoSerie:
    """Envoltura de pyserial que se reconecta sola si el puerto no existe o se desconecta."""

    def __init__(self, nombre, baud=115200):
        self.nombre = nombre
        self.baud = baud
        self._ser = None
        self._lock = threading.Lock()

    def abrir(self):
        import serial   # se importa aqui para que --sin-uart funcione sin pyserial
        try:
            self._ser = serial.Serial(self.nombre, self.baud, timeout=0.2)
            log.info("Puerto %s abierto a %d baud", self.nombre, self.baud)
            return True
        except (serial.SerialException, OSError) as e:
            log.warning("No se pudo abrir %s: %s (reintento en 2 s)", self.nombre, e)
            self._ser = None
            return False

    def write(self, datos):
        with self._lock:
            if self._ser is None:
                raise IOError("puerto cerrado")
            self._ser.write(datos)

    def readline(self):
        ser = self._ser
        if ser is None:
            return b""
        try:
            return ser.readline()
        except Exception as e:   # USB desconectado, etc.
            log.warning("Error leyendo %s: %s", self.nombre, e)
            self.cerrar()
            return b""

    def cerrar(self):
        with self._lock:
            if self._ser is not None:
                try:
                    self._ser.close()
                except Exception:
                    pass
            self._ser = None

    @property
    def abierto(self):
        return self._ser is not None


def hilo_uart(puerto, supervisor, detener):
    while not detener.is_set():
        if not puerto.abierto and not puerto.abrir():
            detener.wait(2.0)
            supervisor.revisar_enlace()
            continue
        crudo = puerto.readline()
        if crudo:
            supervisor.procesar_linea(crudo.decode("ascii", errors="replace"))
        supervisor.revisar_enlace()


def main():
    p = argparse.ArgumentParser(description="Supervisor de la banda clasificadora (Raspberry Pi)")
    p.add_argument("--puerto", default="/dev/serial0", help="puerto serie del ESP32 (por defecto /dev/serial0)")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--sin-uart", action="store_true", help="no abrir el puerto serie (solo panel y base de datos)")
    p.add_argument("--host", default="0.0.0.0", help="direccion donde escucha el panel (0.0.0.0 = toda la red)")
    p.add_argument("--http", type=int, default=8080, help="puerto HTTP del panel (por defecto 8080)")
    p.add_argument("--db", default=os.path.join(AQUI, "banda.db"), help="archivo SQLite")
    p.add_argument("--web", default=os.path.join(AQUI, "..", "web"), help="carpeta del panel web")
    p.add_argument("-v", "--verbose", action="store_true", help="mostrar tambien LATIDO/VIVO y peticiones HTTP")
    args = p.parse_args()

    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO,
                        format="%(asctime)s %(levelname)-7s %(message)s", datefmt="%H:%M:%S")

    db = BaseDatos(args.db)
    puerto = PuertoSerie(args.puerto, args.baud)
    supervisor = Supervisor(db, puerto.write)
    detener = threading.Event()

    if not args.sin_uart:
        threading.Thread(target=hilo_uart, args=(puerto, supervisor, detener), daemon=True, name="uart").start()
    else:
        log.info("Modo --sin-uart: no se abre ningun puerto serie")

    servidor = crear_servidor(args.host, args.http, supervisor, args.web)
    log.info("Base de datos: %s", os.path.abspath(args.db))
    log.info("Panel web en http://%s:%d", "localhost" if args.host in ("0.0.0.0", "") else args.host, args.http)

    def salir(*_):
        detener.set()
        threading.Thread(target=servidor.shutdown, daemon=True).start()

    signal.signal(signal.SIGTERM, salir)
    signal.signal(signal.SIGINT, salir)
    try:
        servidor.serve_forever()
    finally:
        detener.set()
        time.sleep(0.3)
        puerto.cerrar()
        db.cerrar()
        log.info("Supervisor detenido. Estadisticas: %s", supervisor.stats)
    return 0


if __name__ == "__main__":
    sys.exit(main())
