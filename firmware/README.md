# Firmware del ESP32 — sistema completo

Programa final del ESP32: detecta la pieza con los HC-SR04, detiene la banda, lee el UID con el RC522,
clasifica con su tabla local, mueve la compuerta, confirma la salida, cuenta y reporta a la Raspberry Pi.
Sigue clasificando aunque la Raspberry Pi esté apagada.

> **Estado:** compila (`arduino-cli`, core esp32 3.3.12, 26 % de flash) y las pruebas de la lógica pura
> pasan en la PC. **Todavía no se ha cargado en el ESP32 ni se ha probado con el hardware.**
> Los valores [PROVISIONAL] de `config.h` no están medidos.

## 1. Antes de cargarlo

Cada módulo debe haber pasado su prueba individual (`pruebas/README.md`). Si algo falla aquí, vuelve a la
prueba del módulo para aislar la causa. Cableado: [docs/conexiones.md](../docs/conexiones.md).

**Sin el botón de paro instalado**, GPIO39 queda sin pull-down y el firmware puede detectar un paro falso. Para
probar en la mesa sin el botón: une GPIO25 con STBY y con GPIO39 mediante un cable, y pon la resistencia de
10 kΩ de ese nodo a GND (así el firmware ve "paro libre").

## 2. Compilar y cargar

Bibliotecas: **MFRC522** (miguelbalboa) y **Adafruit SSD1306** (instala también Adafruit GFX y Adafruit BusIO).

**Arduino IDE:** abre `banda_clasificadora/banda_clasificadora.ino`, placa *ESP32 Dev Module*, instala las
bibliotecas desde *Administrar bibliotecas* y carga.

**arduino-cli:**
```bash
arduino-cli lib install "MFRC522" "Adafruit SSD1306"
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/banda_clasificadora
arduino-cli upload  --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 firmware/banda_clasificadora
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

**PlatformIO:** desde `firmware/banda_clasificadora/`: `pio run -t upload` y `pio device monitor`.

Recuerda (`conexiones.md` §2.3): mientras programas por USB, desconecta el +5 V de la fuente que va a `VIN`
pero deja la GND común.

## 3. Uso

Al encender, la banda queda en **PAUSA** (LED amarillo). Pulsa **Restablecer** para arrancar.

Monitor Serial (115200), útil en pruebas de mesa:

| Tecla | Acción |
|---|---|
| `s` | Estado completo: ciclo, sensores con distancia, RC522, OLED, paro, enlace, seq, pendientes, conteos, contenedores |
| `t` | Tabla local |
| `r` | Igual que el botón Restablecer (arranque, restablecer, confirmar vaciado) |
| `e` | Paro de emergencia por software |
| `d` | Mostrar/ocultar las distancias de los HC-SR04 cada 500 ms |
| `l` | Mostrar/ocultar las tramas rutinarias (LATIDO, ESTADO, VIVO) |
| `h` | Ayuda |

Qué se guarda en memoria no volátil (NVS, sobrevive a reinicios): último `seq`, conteos A/B/R y sin leer,
ocupación y capacidad de cada contenedor, tabla local y la cola de eventos sin ACK.

## 4. Estructura

| Archivo | Contenido | ¿Se prueba en la PC? |
|---|---|---|
| `banda_clasificadora.ino` | Máquina de estados ([docs/maquina-de-estados.md](../docs/maquina-de-estados.md)), comandos de la Pi, Monitor Serial, arranque | No |
| `config.h` | Pines, tiempos y umbrales | — |
| `protocolo.*` | Checksum, tramas y comandos ([docs/protocolo-uart.md](../docs/protocolo-uart.md)) | Sí |
| `tabla.*` | Tabla local UID → categoría | Sí |
| `pendientes.*` | Cola de eventos sin ACK | Sí |
| `deteccion.*` | Distancia y filtro de presencia | Sí |
| `ultrasonico.*` | Dos HC-SR04, medidos por turnos, con interrupciones (no bloquea) | No |
| `actuadores.*` | Motor (PWMA) y servo | No |
| `interfaz.*` | Paro, botón Restablecer, LEDs, buzzer activo, OLED | No |
| `nfc.*` | RC522 | No |
| `enlace.*` | UART: envío, recepción, latido, reintentos | No |

## 5. Pruebas de la lógica pura (en la PC)

```bash
./firmware/test/correr_pruebas.sh
```
Necesita `g++`. Resultado esperado: `TODAS LAS PRUEBAS PASARON` (90 verificaciones al momento de escribir esto).

## 6. Calibración (con el montaje real)

1. **Umbrales HC-SR04:** `pruebas/ultrasonico_test`, modo `c`. Copia los umbrales sugeridos a
   `UMBRAL_INSP_CM` y `UMBRAL_SALIDA_CM`.
2. **Velocidad:** `pruebas/motor_test`. Elige el `PWM_BANDA_PCT` más bajo con el que la banda arranca con carga.
3. **Tiempos:** con la banda a esa velocidad mide (cronómetro o el `ciclo_ms` del panel):
   - inspección → salida: `SALIDA_TIMEOUT_MS` ≈ el doble del tiempo medido;
   - salida → caída en el contenedor: `RETORNO_CENTRO_MS`;
   - un atasco "razonable": `ATASCO_MS`.
4. **Lectura NFC:** si la etiqueta queda antes o después del RC522 al detenerse la banda, ajusta
   `RETARDO_PARO_MS` o mueve el sensor de inspección.
5. **Ángulos del servo:** `SERVO_DUTY_A/R/B` con la compuerta montada (`pruebas/servo_test`).

Anota cada valor medido (fecha, condición, resultado) en la documentación mecánica del módulo A.

## 7. Pruebas del sistema

Los casos de `pruebas/integracion_test/README.md` siguen valiendo, ahora con sensores y motor reales, más las
pruebas del avance §8: 50 ciclos con piezas A, B y sin etiqueta; atasco, etiqueta ausente, interrupción de
alimentación y desconexión de la Raspberry Pi. El panel exporta el historial completo en CSV como evidencia.

## 8. Limitaciones

- El servo no tiene retroalimentación: la posición de la compuerta solo se verifica indirectamente con el
  sensor de salida (y a simple vista).
- El paro por hardware corta el motor, **no** el servo (este se lleva al centro por software).
- Piezas muy juntas pueden terminar en rechazo `SIN_LECTURA` (ver `docs/maquina-de-estados.md` §3).
- GPIO39 (detección del paro) es una de las entradas del ESP32 con fallos conocidos de interrupciones falsas
  cuando se usa WiFi o el ADC; este firmware no usa ninguno de los dos. Si aparecen paros falsos, revisa el
  cableado del botón antes que el código.
