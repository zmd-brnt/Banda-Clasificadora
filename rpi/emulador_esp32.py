#!/usr/bin/env python3
"""ESP32 emulado: habla el mismo protocolo UART que el firmware real, sin hardware.

Sirve para desarrollar y probar el servidor de la Pi y el panel sin la placa. NO sustituye
las pruebas con el ESP32 real: no hay sensores, motor ni NFC; las piezas se inventan.

Uso:
    python3 emulador_esp32.py                  # crea un puerto virtual /dev/pts/N (Linux/macOS)
    python3 servidor.py --puerto /dev/pts/N    # en otra terminal

Teclas (escribir y Enter en la terminal del emulador):
    c  enlace caido 10 s (el emulador deja de transmitir y acumula pendientes)
    e  paro de emergencia          r  restablecer
    n  pieza sin etiqueta          u  pieza con etiqueta no registrada
    p  pausar / reanudar la generacion de piezas
    s  estado del emulador

Las etiquetas del emulador (UIDS) son INVENTADAS. Registralas en el panel para que se
clasifiquen en A o B; mientras no esten registradas iran a rechazo NO_REGISTRADA.
"""

import argparse
import os
import random
import select
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from banda import protocolo   # noqa: E402

UIDS = ["04A31FB26C8000", "047B22C16C8000", "0419E05A6C8000", "04C4930D6C8000",
        "045E71F86C8000", "04880A3E6C8000", "04D26B916C8000"]
UID_DESCONOCIDO = "04FFEEDDCCBB00"


class EmuladorEsp32:
    """Logica del ESP32 (lado protocolo). escribir(bytes) envia hacia la Pi."""

    def __init__(self, escribir, perder=0.0, semilla=None):
        self.escribir = escribir
        self.perder = perder                 # probabilidad de "perder" una trama PIEZA
        self.rnd = random.Random(semilla)
        self.seq = 0
        self.tabla = {}
        self.version = 0
        self.pendientes = {}                 # seq -> [cuerpo_base, ultimo_envio, envios]
        self.contenido = {"A": 0, "B": 0, "R": 0}
        self.capacidad = {"A": 15, "B": 15, "R": 15}
        self.estado = "ESPERA"
        self.enlace = False
        self.ultimo_rx = 0.0
        self.caido_hasta = 0.0               # simulacion de cable desconectado
        self.ultimo_latido = 0.0
        self.recibidos = []                  # cuerpos recibidos (para pruebas)

    # ------------------------------------------------------------------ envio
    def _tx(self, cuerpo, ahora):
        if ahora < self.caido_hasta:
            return
        self.escribir(protocolo.trama(cuerpo))

    def _tx_pieza(self, seq, ahora):
        base, _, envios = self.pendientes[seq]
        self.pendientes[seq] = [base, ahora, envios + 1]
        cuerpo = f"{base},{1 if envios > 0 else 0}"
        if envios == 0 and self.rnd.random() < self.perder:
            return   # se "pierde" en el cable: llegara por reintento
        self._tx(cuerpo, ahora)

    def alerta(self, codigo, detalle=None, ahora=None):
        ahora = time.monotonic() if ahora is None else ahora
        self._tx(f"ALERTA,{codigo}" + (f",{detalle}" if detalle else ""), ahora)

    def cambiar_estado(self, estado, ahora=None):
        ahora = time.monotonic() if ahora is None else ahora
        self.estado = estado
        self._tx(f"ESTADO,{estado}", ahora)

    def contenedores(self, ahora=None):
        ahora = time.monotonic() if ahora is None else ahora
        c, k = self.contenido, self.capacidad
        self._tx(f"CONTENEDORES,{c['A']},{c['B']},{c['R']},{k['A']},{k['B']},{k['R']}", ahora)

    # ------------------------------------------------------------------ piezas
    def pieza(self, uid, ahora=None, ciclo_ms=None):
        """uid: str o None (sin etiqueta). Devuelve el seq asignado."""
        ahora = time.monotonic() if ahora is None else ahora
        if uid is None:
            cat, motivo, uid_txt = "R", "SIN_LECTURA", "-"
        elif uid in self.tabla:
            cat, motivo, uid_txt = self.tabla[uid], "OK", uid
        else:
            cat, motivo, uid_txt = "R", "NO_REGISTRADA", uid
        self.seq += 1
        self.contenido[cat] += 1
        ciclo = ciclo_ms if ciclo_ms is not None else self.rnd.randint(2500, 4500)
        if self.enlace:
            self.pendientes[self.seq] = [f"PIEZA,{self.seq},{uid_txt},{cat},{motivo},{ciclo}", 0.0, 0]
            self._tx_pieza(self.seq, ahora)
        else:   # como el firmware: un evento que espera al enlace se envia marcado como reenvio
            self.pendientes[self.seq] = [f"PIEZA,{self.seq},{uid_txt},{cat},{motivo},{ciclo}", ahora, 1]
        self.contenedores(ahora)
        return self.seq

    # ------------------------------------------------------------------ recepcion
    def recibir(self, linea, ahora=None):
        ahora = time.monotonic() if ahora is None else ahora
        if ahora < self.caido_hasta:
            return
        cuerpo = protocolo.validar(linea)
        if cuerpo is None:
            return
        self.recibidos.append(cuerpo)
        self.ultimo_rx = ahora
        if not self.enlace:
            self.enlace = True
            self.cambiar_estado(self.estado, ahora)
            self.contenedores(ahora)
        tipo, _, resto = cuerpo.partition(",")
        if tipo == "ACK":
            self.pendientes.pop(int(resto), None)
        elif tipo == "SYNC":
            version, _, lista = resto.partition(",")
            tabla = {}
            for par in filter(None, lista.split(";")):
                uid, _, cat = par.partition("=")
                tabla[uid] = cat
            self.tabla, self.version = tabla, int(version)
            self.alerta("SYNC_OK", version, ahora)
        elif tipo == "REENVIAR":
            seq = int(resto)
            if seq in self.pendientes:
                self._tx_pieza(seq, ahora)
            else:
                self.alerta("SEQ_NO_DISPONIBLE", str(seq), ahora)
        elif tipo == "VACIADO":
            self.contenido[resto] = 0
            self.contenedores(ahora)
        elif tipo == "CAPACIDAD":
            cat, _, n = resto.partition(",")
            self.capacidad[cat] = int(n)
            self.contenedores(ahora)
        elif cuerpo == "PARO":
            self.alerta("EMERGENCIA", "PANEL", ahora)
            self.cambiar_estado("EMERGENCIA", ahora)

    # ------------------------------------------------------------------ periodico
    def tick(self, ahora=None):
        ahora = time.monotonic() if ahora is None else ahora
        if self.enlace and ahora - self.ultimo_rx > 3.0:
            self.enlace = False
        if ahora - self.ultimo_latido >= 1.0:
            self.ultimo_latido = ahora
            self._tx(f"LATIDO,{len(self.pendientes)},{self.version},{self.seq}", ahora)
        if self.enlace:
            for seq, (_, ultimo, envios) in list(self.pendientes.items()):
                if envios == 0 or ahora - ultimo >= 1.0:
                    self._tx_pieza(seq, ahora)


def main():
    p = argparse.ArgumentParser(description="ESP32 emulado para probar el supervisor sin hardware")
    p.add_argument("--intervalo", type=float, default=4.0, help="segundos entre piezas (por defecto 4)")
    p.add_argument("--perder", type=float, default=0.0, help="probabilidad 0..1 de perder una trama PIEZA")
    args = p.parse_args()

    import termios
    import tty

    maestro, esclavo = os.openpty()
    tty.setraw(esclavo)
    attrs = termios.tcgetattr(esclavo)
    attrs[3] &= ~termios.ECHO
    termios.tcsetattr(esclavo, termios.TCSANOW, attrs)
    nombre = os.ttyname(esclavo)
    print(f"ESP32 emulado listo en {nombre}")
    print(f"  En otra terminal:  python3 servidor.py --puerto {nombre}")
    print("  Teclas: c=enlace caido 10 s  e=emergencia  r=restablecer  n=sin etiqueta  u=no registrada  "
          "p=pausar  s=estado")

    emu = EmuladorEsp32(lambda b: os.write(maestro, b), perder=args.perder)
    emu.alerta("REINICIO", "ENCENDIDO")
    buf = b""
    pausado = False
    siguiente = time.monotonic() + args.intervalo

    while True:
        listos, _, _ = select.select([maestro, sys.stdin], [], [], 0.1)
        ahora = time.monotonic()
        if maestro in listos:
            buf += os.read(maestro, 4096)
            while b"\n" in buf:
                linea, buf = buf.split(b"\n", 1)
                emu.recibir(linea.decode("ascii", errors="replace"), ahora)
        if sys.stdin in listos:
            tecla = sys.stdin.readline().strip()
            if tecla == "c":
                emu.caido_hasta = ahora + 10
                print("Enlace caido 10 s")
            elif tecla == "e":
                emu.alerta("EMERGENCIA", "BOTON", ahora)
                emu.cambiar_estado("EMERGENCIA", ahora)
            elif tecla == "r":
                emu.alerta("RESTABLECIDO", None, ahora)
                emu.cambiar_estado("ESPERA", ahora)
            elif tecla == "n":
                print(f"Pieza sin etiqueta: seq {emu.pieza(None, ahora)}")
            elif tecla == "u":
                print(f"Pieza no registrada: seq {emu.pieza(UID_DESCONOCIDO, ahora)}")
            elif tecla == "p":
                pausado = not pausado
                print("Generacion pausada" if pausado else "Generacion reanudada")
            elif tecla == "s":
                print(f"estado={emu.estado} seq={emu.seq} pendientes={sorted(emu.pendientes)} "
                      f"tabla v{emu.version} ({len(emu.tabla)}) enlace={emu.enlace} contenido={emu.contenido}")

        if not pausado and emu.estado == "ESPERA" and ahora >= siguiente:
            siguiente = ahora + args.intervalo
            uid = None if emu.rnd.random() < 0.1 else emu.rnd.choice(UIDS)
            for e in ("LECTURA_NFC", "CLASIFICANDO", "LIBERA_INSPECCION", "CONFIRMA_SALIDA", "RETORNO"):
                emu.cambiar_estado(e, ahora)
            seq = emu.pieza(uid, ahora)
            emu.cambiar_estado("ESPERA", ahora)
            print(f"Pieza seq {seq}: {uid or 'sin etiqueta'}")
        emu.tick(ahora)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
