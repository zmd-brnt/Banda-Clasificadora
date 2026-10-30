# Pruebas de Módulos Individuales

Cada carpeta contiene una prueba independiente para verificar un componente
antes de integrar el sistema completo.

## Orden recomendado

1. **servo_test** - Solo necesita el ESP32 y el servo. Sin dependencias externas.
2. **nfc_test** - Necesita el ESP32 y el RC522. Verifica la conexión SPI.
3. **ir_test** - Necesita el ESP32 y los dos sensores IR (GPIO34 y GPIO35). Mide el voltaje de cada salida y, con la prueba guiada (`g`), diagnostica alimentación, lógica invertida, oscilación y acoplamiento entre sensores.
4. **uart_test** - Necesita el ESP32 y la Raspberry Pi conectados. Verifica la comunicación serial.
5. **integracion_test** - RC522 + servo + UART funcionando juntos siguiendo el ciclo de clasificación del proyecto (sin IR ni motor; la etiqueta se acerca y se retira a mano). Requiere que las pruebas anteriores ya pasen. Ver [integracion_test/README.md](integracion_test/README.md).

## Requisitos comunes

- Arduino IDE con soporte para ESP32 instalado
  - En Preferences -> Board Manager URLs agregar:
    `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
  - En Board Manager buscar "esp32" e instalar "esp32 by Espressif Systems"
- Seleccionar placa: "ESP32 Dev Module"
- Velocidad del Monitor Serial: 115200 baud
