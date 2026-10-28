# Pruebas de Módulos Individuales

Cada carpeta contiene una prueba independiente para verificar un componente
antes de integrar el sistema completo.

## Orden recomendado

1. **servo_test** - Solo necesita el ESP32 y el servo. Sin dependencias externas.
2. **nfc_test** - Necesita el ESP32 y el RC522. Verifica la conexión SPI.
3. **uart_test** - Necesita el ESP32 y la Raspberry Pi conectados. Verifica la comunicación serial.

## Requisitos comunes

- Arduino IDE con soporte para ESP32 instalado
  - En Preferences -> Board Manager URLs agregar:
    `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
  - En Board Manager buscar "esp32" e instalar "esp32 by Espressif Systems"
- Seleccionar placa: "ESP32 Dev Module"
- Velocidad del Monitor Serial: 115200 baud
