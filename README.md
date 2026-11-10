# Banda-Clasificadora
Creación de un sistema de clasificación de objetos usando lector de NFC.

## Estructura

- `firmware/` — programa final del ESP32 (máquina de estados, HC-SR04, RC522, motor, compuerta, OLED, UART) y sus pruebas en la PC. Ver [firmware/README.md](firmware/README.md).
- `rpi/` — servidor de la Raspberry Pi: UART, base de datos SQLite y panel web en vivo; incluye un ESP32 emulado y pruebas. Ver [rpi/README.md](rpi/README.md).
- `web/` — panel web; funciona con datos reales (servido por `rpi/servidor.py`) o como simulador. Ver [web/README.md](web/README.md).
- `pruebas/` — pruebas de módulos individuales en el hardware. Ver [pruebas/README.md](pruebas/README.md).
- `docs/` — documentos de avance, [conexiones del hardware](docs/conexiones.md), [protocolo UART](docs/protocolo-uart.md), [máquina de estados](docs/maquina-de-estados.md) y [plan de trabajo del equipo](docs/plan-de-trabajo-equipo.md).

## Estado

El software de todo el sistema está escrito y probado **solo en la PC** (pruebas automáticas y ESP32 emulado).
Falta cargarlo y probarlo en el hardware, y medir los valores marcados como [PROVISIONAL] en
`firmware/banda_clasificadora/config.h` (ver [firmware/README.md](firmware/README.md) §6).

## Puesta en marcha rápida

1. Probar cada módulo en el orden de [docs/conexiones.md](docs/conexiones.md) §7.
2. Cargar `firmware/banda_clasificadora` en el ESP32.
3. En la Raspberry Pi: `cd rpi && python3 servidor.py` y abrir `http://<ip-de-la-pi>:8080`.
4. Registrar las etiquetas en el panel (pestaña *Etiquetas*); la tabla se envía sola al ESP32.
5. Pulsar **Restablecer** en la banda para arrancar.
