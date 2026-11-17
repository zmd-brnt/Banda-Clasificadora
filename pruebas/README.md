# Pruebas de Módulos Individuales

Cada carpeta contiene una prueba independiente para verificar un componente
antes de integrar el sistema completo (`firmware/banda_clasificadora`).

> Las pruebas `oled_test`, `ultrasonico_test`, `motor_test`, `paro_test` e `indicadores_test` son **nuevas**:
> compilan (`arduino-cli`, core esp32 3.3.12) pero **todavía no se han probado en el hardware**.
> El orden y los criterios de éxito están en [docs/conexiones.md](../docs/conexiones.md) §7.

## Orden recomendado

1. **servo_test** - Compuerta doble: ESP32 + 2 servos (GPIO13 y GPIO12). Calibra los ángulos que usan la integración y el firmware. Sin dependencias externas.
2. **nfc_test** - Necesita el ESP32 y el RC522. Verifica la conexión SPI.
3. **oled_test** - Escáner I2C (debe aparecer `0x3C` o `0x3D`) y texto de prueba. Biblioteca: Adafruit SSD1306.
4. **ultrasonico_test** - Los dos HC-SR04. **Antes de conectar `Echo` al ESP32** mide el divisor con multímetro (instrucciones en el encabezado del `.ino`). Modos: `m` monitor de distancias, `c` calibración guiada (sugiere los umbrales para `config.h`), `x` prueba de interferencia entre sensores, `e` estados del objeto (entrada → tránsito → salida, con tiempos y alertas).
5. **motor_test** - TB6612 + motor, **sin botón de paro** (GPIO25 directo a STBY). Arranca solo al 100 %. `s` detener, `f` 100 %, `0`–`9` velocidad (0 a 90 %), `d` PWMA fijo en ALTO sin PWM (diagnóstico), `a` prueba automática (30, 60 y 100 %). Sirve para elegir `PWM_BANDA_PCT`.
6. **paro_test** - Botón de paro. `m` monitor, `b` **congela el programa a propósito** con el motor girando: el motor debe detenerse al presionar el botón aunque el programa no responda (demuestra el paro por hardware), `s` estado.
7. **indicadores_test** - LEDs, buzzer **activo** y botón Restablecer (con antirrebote).
8. **uart_test** - Necesita el ESP32 y la Raspberry Pi conectados. Verifica la comunicación serial.
9. **integracion_test** - RC522 + compuerta doble (2 servos) + UART funcionando juntos siguiendo el ciclo de clasificación del proyecto (sin sensores de presencia ni motor; la etiqueta se acerca y se retira a mano). Requiere que las pruebas anteriores ya pasen. Ver [integracion_test/README.md](integracion_test/README.md).
10. **Sistema completo** - [firmware/README.md](../firmware/README.md) y [rpi/README.md](../rpi/README.md).

**ir_test** - **OBSOLETO:** los sensores IR se sustituyeron por dos HC-SR04. Se conserva solo como referencia; no usarlo con los HC-SR04 (un `Echo` de 5 V conectado a GPIO34/35 sin divisor puede dañar el ESP32).

## Requisitos comunes

- Arduino IDE con soporte para ESP32 instalado
  - En Preferences -> Board Manager URLs agregar:
    `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
  - En Board Manager buscar "esp32" e instalar "esp32 by Espressif Systems"
- Seleccionar placa: "ESP32 Dev Module"
- Velocidad del Monitor Serial: 115200 baud
- Bibliotecas: MFRC522 (nfc, integración, firmware) y Adafruit SSD1306 + Adafruit GFX (oled, firmware)

## Pruebas automáticas (sin hardware)

| Qué | Comando |
|---|---|
| Lógica pura del firmware (protocolo, tabla, pendientes, filtro de presencia) | `./firmware/test/correr_pruebas.sh` |
| Servidor de la Raspberry Pi (protocolo, SQLite, supervisor + ESP32 emulado, HTTP) | `cd rpi && python3 -m unittest discover -s tests -t .` |
