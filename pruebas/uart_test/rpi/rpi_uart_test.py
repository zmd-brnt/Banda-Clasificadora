#!/usr/bin/env python3
"""
Prueba UART: Raspberry Pi <-> ESP32

Conexiones:
  RPi GPIO14 (TXD / pin fisico 8)  ----> ESP32 GPIO16 (RX2)
  RPi GPIO15 (RXD / pin fisico 10) <---- ESP32 GPIO17 (TX2)
  RPi GND    (pin fisico 6)        ----- ESP32 GND

Requisitos:
  1. Habilitar UART en la Raspberry Pi:
     sudo raspi-config -> Interface Options -> Serial Port
       - Login shell over serial: NO
       - Serial port hardware enabled: YES
     Reiniciar la Pi despues de este cambio.

  2. Instalar pyserial:
     pip3 install pyserial

Funcionamiento:
  Escucha mensajes "PING:<n>" del ESP32 por /dev/serial0.
  Responde con "PONG:<n>" inmediatamente.

Criterio de exito:
  - La terminal muestra "Recibido: PING:1" -> "Enviado:  PONG:1", etc.
  - El Monitor Serial del ESP32 muestra los PONG correspondientes.
"""

import serial
import sys

PUERTO = "/dev/serial0"
BAUD = 115200

def main():
    print("=== Prueba UART Raspberry Pi ===")
    print(f"Puerto: {PUERTO}  Baud: {BAUD}")
    print("Esperando mensajes PING del ESP32...")
    print()

    try:
        ser = serial.Serial(PUERTO, BAUD, timeout=5)
    except serial.SerialException as e:
        print(f"Error al abrir {PUERTO}: {e}")
        print()
        print("Verifica que:")
        print("  1. El UART esta habilitado (sudo raspi-config)")
        print("  2. No hay otro proceso usando el puerto")
        print("  3. Tu usuario tiene permisos (grupo 'dialout')")
        print("     sudo usermod -a -G dialout $USER")
        sys.exit(1)

    contador = 0

    try:
        while True:
            linea = ser.readline().decode("utf-8", errors="replace").strip()

            if not linea:
                continue

            contador += 1
            print(f"[{contador}] Recibido: {linea}")

            if linea.startswith("PING:"):
                seq = linea.split(":", 1)[1]
                respuesta = f"PONG:{seq}\n"
                ser.write(respuesta.encode("utf-8"))
                print(f"[{contador}] Enviado:  PONG:{seq}")
            else:
                print(f"[{contador}] Mensaje no reconocido, ignorado")

            print("---")

    except KeyboardInterrupt:
        print("\nPrueba terminada por el usuario.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
