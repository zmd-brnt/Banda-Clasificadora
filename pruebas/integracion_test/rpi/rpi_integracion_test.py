#!/usr/bin/env python3
"""
Prueba de integracion: Raspberry Pi <-> ESP32 (NFC + Servo + UART)

Conexiones:
  RPi GPIO14 (TXD / pin fisico 8)  ----> ESP32 GPIO16 (RX2)
  RPi GPIO15 (RXD / pin fisico 10) <---- ESP32 GPIO17 (TX2)
  RPi GND    (pin fisico 6)        ----- ESP32 GND

Requisitos:
  - UART habilitado (ver pruebas/uart_test/rpi/rpi_uart_test.py)
  - pip3 install pyserial

Funcionamiento:
  - Es la "tabla maestra": envia SYNC al ESP32 cuando su version no coincide.
  - Responde VIVO a cada LATIDO (el ESP32 lo usa para saber si el enlace sigue).
  - Confirma cada PIEZA con ACK, descarta duplicados por seq y pide REENVIAR los huecos.
  - Muestra los cambios de ESTADO y las ALERTA del ESP32.
  - Guarda el historial en historial_prueba.csv (evidencia para el reporte).

Uso:
  python3 rpi_integracion_test.py                 # normal
  python3 rpi_integracion_test.py --perder-ack 3  # no responde 1 de cada 3 ACK (prueba de reintentos)
  python3 rpi_integracion_test.py --reiniciar     # borra el historial de la prueba

Antes de correrlo, llena TABLA con los UID que muestra nfc_test
(mayusculas, sin ':'; ejemplo: 04A1B2C3D4E5F6).
"""

import argparse
import csv
import os
import sys
import time
import zlib
from datetime import datetime

import serial

PUERTO = "/dev/serial0"
BAUD = 115200
LATIDO_TIMEOUT_S = 3.0
ARCHIVO_HISTORIAL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "historial_prueba.csv")
COLUMNAS = ["seq", "fecha_hora", "uid", "categoria", "motivo", "ciclo_ms", "reenvio"]

ALERTAS = {
    "REINICIO": "el ESP32 arranco (encendido o reinicio)",
    "ATASCO": "la pieza no salio del lector en 10 s; ESP32 en ERROR hasta 'r'",
    "PIEZAS_JUNTAS": "llego otra pieza antes de terminar la anterior",
    "EMERGENCIA": "paro de emergencia; compuerta al centro",
    "RESTABLECIDO": "el operador restablecio el sistema",
    "NFC_FALLA": "el ESP32 no detecta el RC522",
    "PENDIENTES_PERDIDOS": "se lleno el buffer de pendientes del ESP32; se perdio un evento",
}

# Tabla maestra: UID -> categoria ("A", "B" o "R")
TABLA = {
    # "04A1B2C3D4E5F6": "A",
    # "04112233445566": "B",
}


def version_tabla(tabla):
    """Version derivada del contenido: cambia sola cuando se edita TABLA."""
    texto = ";".join(f"{uid}={cat}" for uid, cat in sorted(tabla.items()))
    return (zlib.crc32(texto.encode("ascii")) & 0xFFFFFFFF) or 1


def checksum(cuerpo):
    x = 0
    for c in cuerpo.encode("ascii", errors="replace"):
        x ^= c
    return x


def enviar(ser, cuerpo, mostrar=True):
    ser.write(f"{cuerpo}*{checksum(cuerpo):02X}\n".encode("ascii"))
    if mostrar:
        print(f"  [UART >] {cuerpo}")


def validar(linea):
    """Devuelve el cuerpo de la trama o None si el checksum no coincide."""
    if len(linea) < 4 or linea[-3] != "*":
        return None
    cuerpo, hh = linea[:-3], linea[-2:]
    try:
        return cuerpo if int(hh, 16) == checksum(cuerpo) else None
    except ValueError:
        return None


def cargar_historial(reiniciar):
    if reiniciar and os.path.exists(ARCHIVO_HISTORIAL):
        os.remove(ARCHIVO_HISTORIAL)
    vistos = set()
    if os.path.exists(ARCHIVO_HISTORIAL):
        with open(ARCHIVO_HISTORIAL, newline="") as f:
            for fila in csv.DictReader(f):
                vistos.add(int(fila["seq"]))
    else:
        with open(ARCHIVO_HISTORIAL, "w", newline="") as f:
            csv.writer(f).writerow(COLUMNAS)
    return vistos


def guardar_pieza(campos):
    with open(ARCHIVO_HISTORIAL, "a", newline="") as f:
        csv.writer(f).writerow(campos)


def main():
    parser = argparse.ArgumentParser(description="Prueba de integracion RPi <-> ESP32")
    parser.add_argument("--perder-ack", type=int, default=0, metavar="N",
                        help="no enviar 1 de cada N ACK para provocar reenvios")
    parser.add_argument("--reiniciar", action="store_true", help="borrar historial_prueba.csv")
    args = parser.parse_args()

    for uid, cat in TABLA.items():
        if cat not in ("A", "B", "R") or not uid or ":" in uid or uid != uid.upper():
            print(f"Entrada invalida en TABLA: {uid!r} -> {cat!r}")
            sys.exit(1)

    version = version_tabla(TABLA)
    payload_sync = f"SYNC,{version}," + ";".join(f"{u}={c}" for u, c in sorted(TABLA.items()))

    print("=== Prueba de integracion - Raspberry Pi ===")
    print(f"Puerto: {PUERTO}  Baud: {BAUD}")
    print(f"Tabla maestra v{version}: {len(TABLA)} etiquetas")
    if args.perder_ack:
        print(f"MODO PRUEBA: se omitira 1 de cada {args.perder_ack} ACK")
    print()

    try:
        ser = serial.Serial(PUERTO, BAUD, timeout=0.5)
    except serial.SerialException as e:
        print(f"Error al abrir {PUERTO}: {e}")
        print("Revisa raspi-config, que no haya otro proceso usando el puerto y el grupo 'dialout'.")
        sys.exit(1)

    vistos = cargar_historial(args.reiniciar)
    max_seq = max(vistos, default=0)
    conteo = {"A": 0, "B": 0, "R": 0, "SIN_LECTURA": 0, "NO_REGISTRADA": 0}
    stats = {"piezas": 0, "duplicados": 0, "invalidas": 0, "acks_omitidos": 0}
    acks_recibibles = 0
    ultimo_latido = None
    esp32_activo = False

    if vistos:
        print(f"Historial previo: {len(vistos)} piezas, ultimo seq {max_seq}")
    print("Esperando tramas del ESP32... (Ctrl+C para terminar)")
    print()

    try:
        while True:
            crudo = ser.readline()
            ahora = time.monotonic()

            if esp32_activo and ultimo_latido and ahora - ultimo_latido > LATIDO_TIMEOUT_S:
                esp32_activo = False
                print(">> Sin LATIDO del ESP32 en 3 s: enlace CAIDO")

            if not crudo:
                continue

            linea = crudo.decode("ascii", errors="replace").strip()
            cuerpo = validar(linea)
            if cuerpo is None:
                stats["invalidas"] += 1
                print(f"  [UART <] Trama invalida descartada: {linea!r}")
                continue

            campos = cuerpo.split(",")
            tipo = campos[0]

            numericos = {"LATIDO": (1, 2), "PIEZA": (1, 5)}.get(tipo, ())
            if any(i >= len(campos) or not campos[i].isdigit() for i in numericos):
                stats["invalidas"] += 1
                print(f"  [UART <] Campos numericos invalidos: {cuerpo}")
                continue

            if tipo == "LATIDO" and len(campos) == 3:
                ultimo_latido = ahora
                if not esp32_activo:
                    esp32_activo = True
                    print(">> ESP32 ACTIVO")
                pendientes, version_esp = int(campos[1]), int(campos[2])
                if pendientes:
                    print(f"  [LATIDO] el ESP32 tiene {pendientes} eventos pendientes")
                enviar(ser, f"VIVO,{version}", mostrar=False)
                if version_esp != version:
                    print(f"  [SYNC] ESP32 tiene v{version_esp}, se envia v{version}")
                    enviar(ser, payload_sync)

            elif tipo == "PIEZA" and len(campos) == 7 and campos[3] in ("A", "B", "R"):
                seq = int(campos[1])
                uid, cat, motivo, ciclo_ms, reenvio = campos[2], campos[3], campos[4], campos[5], campos[6]
                etiqueta = " (reenvio)" if reenvio == "1" else ""
                print(f"[PIEZA seq {seq}]{etiqueta} UID {uid}  cat {cat}  {motivo}  ciclo {ciclo_ms} ms")

                if seq in vistos:
                    stats["duplicados"] += 1
                    print("  Duplicado: ya registrado, se descarta (pero se confirma)")
                else:
                    if seq > max_seq + 1:
                        faltan = [s for s in range(max_seq + 1, seq) if s not in vistos]
                        print(f"  HUECO: faltan seq {faltan}, se pide reenvio")
                        for s in faltan:
                            enviar(ser, f"REENVIAR,{s}")
                    elif seq < max_seq:
                        print("  Evento atrasado recuperado")
                    vistos.add(seq)
                    max_seq = max(max_seq, seq)
                    stats["piezas"] += 1
                    conteo[cat] += 1
                    if motivo in conteo:
                        conteo[motivo] += 1
                    guardar_pieza([seq, datetime.now().isoformat(timespec="seconds"),
                                   uid, cat, motivo, ciclo_ms, reenvio])

                acks_recibibles += 1
                if args.perder_ack and acks_recibibles % args.perder_ack == 0:
                    stats["acks_omitidos"] += 1
                    print(f"  (ACK de seq {seq} omitido a proposito)")
                else:
                    enviar(ser, f"ACK,{seq}")

                print(f"  Conteo de esta sesion A/B/R: {conteo['A']} / {conteo['B']} / {conteo['R']}"
                      f"  (sin leer {conteo['SIN_LECTURA']}, no registradas {conteo['NO_REGISTRADA']})")
                print("---")

            elif tipo == "ESTADO" and len(campos) == 2:
                print(f"  [ESTADO] {campos[1]}")

            elif tipo == "ALERTA" and len(campos) == 2:
                print(f"!! ALERTA del ESP32: {campos[1]} - {ALERTAS.get(campos[1], 'codigo desconocido')}")

            else:
                print(f"  [UART <] Trama no reconocida: {cuerpo}")

    except KeyboardInterrupt:
        print()
        print("=== Resumen ===")
        print(f"  Piezas nuevas registradas: {stats['piezas']}")
        print(f"  Conteo A/B/R:              {conteo['A']} / {conteo['B']} / {conteo['R']}")
        print(f"  Duplicados descartados:    {stats['duplicados']}")
        print(f"  Tramas invalidas:          {stats['invalidas']}")
        print(f"  ACK omitidos a proposito:  {stats['acks_omitidos']}")
        print(f"  Historial: {ARCHIVO_HISTORIAL}")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
