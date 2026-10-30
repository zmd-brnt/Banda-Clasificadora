# Prueba de integración: NFC + Servo + UART

Comprueba que el lector RC522, el servo SG90 y el enlace UART con la Raspberry Pi funcionan **juntos**
siguiendo el ciclo de clasificación descrito en el proyecto (§5 y §8 del documento de avance).
**No usa los sensores IR, el motor ni la banda:** la pieza (una etiqueta NFC) se acerca y se retira a mano.

> **Estado:** compilado (`arduino-cli`, core esp32 3.3.12) y script de la Pi probado contra un ESP32
> simulado (huecos, reenvíos, duplicados, alertas). **Todavía no se ha probado en el hardware real.**

## Cambios respecto a la versión anterior

La versión anterior integraba también los dos IR (inspección y salida). Se quitaron para aislar los tres
módulos de esta prueba. Lo que hacían los elementos ausentes se sustituye así:

| Elemento del proyecto | En esta prueba |
|---|---|
| IR de inspección detecta la pieza | La etiqueta **entra al campo del lector**. Para una pieza **sin etiqueta**, el comando `p` |
| Motor: banda se detiene / reanuda | Solo mensajes `[Banda] Detenida` / `[Banda] En marcha` en el Monitor Serial |
| La banda saca la pieza de la inspección | **Retirar la etiqueta del lector** a mano |
| IR de salida confirma el paso | Tiempo fijo de tránsito: 2 s (`TRANSITO_MS`) después de retirar la pieza |
| Atasco (IR de inspección bloqueado) | La etiqueta sigue en el lector **10 s** después de clasificarla |
| Doble lectura (IR sin liberarse) | La etiqueta ya leída queda en *HALT* y no vuelve a disparar el ciclo hasta que sale del campo |
| Piezas muy juntas (IR se reactiva) | Entra otra etiqueta antes de que la compuerta vuelva al centro → `ALERTA,PIEZAS_JUNTAS` |
| Botón de paro / Restablecer | Comandos `e` / `r` del Monitor Serial |

Además se añadió, para acercarse al protocolo que ya espera el panel web (`web/js/estado.js`):

- Trama `ESTADO,<nombre>` en cada cambio de estado, con los mismos nombres que el simulador web.
- La Pi pide `REENVIAR,<seq>` cuando detecta un hueco en la secuencia.
- Alertas `EMERGENCIA`, `RESTABLECIDO` y `PIEZAS_JUNTAS`.
- Conteo A/B/R y "sin leer" guardado en NVS (sobrevive a reinicios, como pide §8 del documento).
- Se quitaron `SIN_SALIDA` (no hay IR de salida) y los pines GPIO34/35.

## Requisitos previos

`servo_test`, `nfc_test` y `uart_test` deben haber pasado. Si algo falla aquí, vuelve a la prueba
individual del módulo para aislar la causa.

| Módulo | Pin del ESP32 | Notas |
|---|---|---|
| RC522 | SS 5, SCK 18, MOSI 23, MISO 19, **RST 4** | Solo 3.3 V |
| Servo SG90 | Señal → **GPIO13** | +5 V de la **rama de potencia**, nunca del ESP32; GND común |
| UART | TX2 **17** → RPi pin 10 · RX2 **16** ← RPi pin 8 · GND ↔ RPi pin 6 | TX con RX, cruzado |

Detalles de alimentación y tierras: [docs/conexiones.md](../../docs/conexiones.md).

**Montaje en la mesa:** fija el RC522 y presenta cada etiqueta a **2 cm o menos** de la antena, siempre en
la misma orientación (es la distancia que se identificó como límite en el documento). Ten a la vista la
compuerta del servo para verificar a ojo hacia dónde apunta.

## Ciclo de una pieza

```
ESPERA ──(etiqueta entra al campo, o 'p')──► LECTURA_NFC   banda "detenida", hasta 1.5 s
   ▲                                              │  tabla local → A / B / R
   │                                              ▼
   │                                        CLASIFICANDO   servo: A 0° · B 180° · R 90°  (0.4 s)
   │                                              │  banda "en marcha"
   │                                              ▼
   │                                        LIBERA_LECTOR  retirar la etiqueta (máx. 10 s → ATASCO)
   │                                              │  (sin etiqueta se salta este paso)
   │                                              ▼
   │                                       CONFIRMA_SALIDA tránsito simulado 2 s → conteo + PIEZA por UART
   │                                              ▼
   └──────────────────────────────────────── RETORNO       1 s después, compuerta al centro

Desde cualquier estado: 'e' → EMERGENCIA.   ATASCO → ERROR.   Ambos salen con 'r' (lector libre).
```

En la práctica: **acerca la etiqueta → espera a ver la compuerta moverse → retírala**. Unos 3 s después
aparece el evento y la compuerta vuelve al centro.

## Cómo ejecutar

1. **Anota los UID.** Con `nfc_test`, lee tus etiquetas y escribe los UID **en mayúsculas y sin `:`**
   (por ejemplo `04:a1:b2:...` se escribe `04A1B2...`) en `TABLA` dentro de `rpi/rpi_integracion_test.py`.
   Asigna al menos una a `"A"`, otra a `"B"` y deja una **sin registrar**.
2. **ESP32:** abre `esp32_integracion_test/` en el Arduino IDE (o `pio run -t upload` desde esa carpeta)
   y cárgalo. Abre el Monitor Serial a 115200.
3. **Raspberry Pi:** copia la carpeta `rpi/` y ejecuta:
   ```bash
   python3 rpi_integracion_test.py --reiniciar
   ```
4. En 1–2 s el ESP32 debe mostrar `Enlace con la Raspberry Pi ACTIVO` y `SYNC aplicado` con tu tabla.

Comandos del Monitor Serial:

| Tecla | Acción |
|---|---|
| `p` | Llega una pieza **sin etiqueta** (simula el IR de inspección) |
| `e` | Paro de emergencia: compuerta al centro, la pieza en curso no se registra |
| `r` | Restablecer tras `ATASCO` o `EMERGENCIA` (el lector debe estar libre) |
| `s` | Estado: ciclo, enlace, seq, pendientes, tabla, conteos, tramas inválidas |
| `t` | Tabla local |
| `l` | Mostrar/ocultar las tramas `LATIDO` y `ESTADO` |

## Protocolo

Texto, una trama por línea: `<cuerpo>*HH`, donde `HH` es el XOR de los caracteres del cuerpo en
hexadecimal. Una trama con checksum incorrecto se descarta.

| Sentido | Trama | Para qué |
|---|---|---|
| ESP32 → RPi | `PIEZA,seq,uid,cat,motivo,ciclo_ms,reenvio` | Evento de clasificación (`motivo`: `OK`, `SIN_LECTURA`, `NO_REGISTRADA`) |
| ESP32 → RPi | `ESTADO,nombre` | Cada cambio de estado del ciclo |
| ESP32 → RPi | `LATIDO,pendientes,version_tabla` | Cada 1 s |
| ESP32 → RPi | `ALERTA,codigo` | `REINICIO`, `ATASCO`, `PIEZAS_JUNTAS`, `EMERGENCIA`, `RESTABLECIDO`, `NFC_FALLA`, `PENDIENTES_PERDIDOS` |
| RPi → ESP32 | `ACK,seq` | Confirma un evento |
| RPi → ESP32 | `VIVO,version` | Responde al latido; sin él durante 3 s el ESP32 marca el enlace como caído |
| RPi → ESP32 | `SYNC,version,uid=cat;uid=cat` | Tabla maestra, solo si la versión no coincide |
| RPi → ESP32 | `REENVIAR,seq` | Hueco en la secuencia: pide reenviar ese evento si sigue pendiente |

El ESP32 guarda en memoria no volátil (NVS) el último `seq`, el conteo y la tabla. Los eventos sin ACK se
reenvían cada 1 s mientras haya enlace; caben hasta 16 en el búfer.

## Casos de prueba y criterios de aceptación

Repite cada caso las veces indicadas y anota el resultado. Guarda la captura del Monitor Serial, la salida
de la Pi y el archivo `rpi/historial_prueba.csv` como evidencia.

| # | Caso | Qué hacer | Resultado esperado | Rep. |
|---|---|---|---|---|
| 1 | Categoría A | Acercar la etiqueta A y retirarla cuando se mueva la compuerta | Compuerta a 0°; ~2 s después de retirarla `PIEZA ... A OK`, ACK; vuelve al centro 1 s después | 10 |
| 2 | Categoría B | Igual, con la etiqueta B | Compuerta a 180°; `B OK` | 10 |
| 3 | No registrada | Etiqueta que no está en `TABLA` | Compuerta al centro; `R NO_REGISTRADA` | 5 |
| 4 | Sin etiqueta | Enviar `p` sin etiqueta en el lector | Tras ~1.5 s: `R SIN_LECTURA`, UID `-` | 5 |
| 5 | Doble lectura | Dejar la etiqueta A quieta 5 s en el lector y luego retirarla | **Un solo** evento | 3 |
| 6 | Atasco | Dejar la etiqueta más de 10 s en el lector | `ATASCO`, compuerta al centro, alerta en la Pi; `r` con la etiqueta puesta es rechazado; al retirarla, `r` vuelve a `ESPERA` y llega `RESTABLECIDO` | 2 |
| 7 | Piezas juntas | Clasificar A y, durante los 3 s de tránsito/retorno, acercar la B | `ALERTA PIEZAS_JUNTAS`; A se registra normal y después B se procesa como pieza nueva | 2 |
| 8 | Emergencia | Acercar una etiqueta y enviar `e` antes de retirarla | Compuerta al centro, `ALERTA EMERGENCIA`, **no** se registra evento; `r` (lector libre) vuelve a `ESPERA` | 2 |
| 9 | ACK perdido | Correr la Pi con `--perder-ack 3` y pasar 6 piezas | Aparecen `[REINTENTO]`; la Pi marca `Duplicado` y el CSV tiene cada seq **una sola vez** | 1 |
| 10 | Pi desconectada | Detener el script (Ctrl+C), pasar 3 piezas, volver a correrlo **sin** `--reiniciar` | El ESP32 clasifica igual con su tabla local y acumula 3 pendientes; al volver la Pi, llegan con ACK y no faltan seq en el CSV | 1 |
| 11 | Reinicio del ESP32 | Pulsar EN con la Pi detenida y pasar una pieza A | `Tabla recuperada de memoria`; el conteo y el `seq` continúan (no vuelven a 0/1); sigue clasificando A | 1 |
| 12 | Tabla nueva | Cambiar una categoría en `TABLA` y reiniciar el script | `SYNC aplicado` con la nueva versión; la pieza va a su nueva categoría | 1 |
| 13 | Estabilidad | 20 piezas seguidas mezclando A, B, no registradas y `p` | El ESP32 no se reinicia (no aparece `ALERTA REINICIO` en la Pi), 0 tramas inválidas (`s`), conteo A/B/R igual en ambos lados | 1 |

**La prueba se aprueba** cuando los casos 1–4 clasifican correctamente en el 100 % de las repeticiones y los
casos 5–13 cumplen el resultado esperado. Si el caso 13 falla porque el ESP32 se reinicia al mover el servo,
el problema es la alimentación (capacitor y rama de potencia, §2 de `conexiones.md`), no el código.

## Limitaciones (decirlas en el reporte)

- **No hay IR de salida:** la confirmación de salida es un tiempo fijo, no una medición. Esta prueba **no**
  puede detectar una pieza que se quede atorada entre la compuerta y el contenedor; eso se valida cuando
  se integren los IR.
- **El lector hace de IR de inspección:** una pieza sin etiqueta no se detecta sola, por eso existe `p`.
  En el sistema real, el IR dispara la lectura y el RC522 solo identifica.
- **Sin motor:** `TRANSITO_MS` (2 s) y el `ciclo_ms` reportado incluyen el tiempo de tu mano; no son
  tiempos reales de la banda. Habrá que recalcularlos con la velocidad de la banda y la distancia
  lector–compuerta.
- **El servo no tiene retroalimentación:** que la compuerta llegue al ángulo correcto se verifica a simple
  vista.
- **La emergencia es un comando por software.** En el sistema final el botón debe actuar por interrupción
  y, idealmente, cortar la alimentación del motor sin depender del programa.
- **Contenedor lleno, OLED, buzzer y LED** no forman parte de esta prueba.
- **Si se borra la NVS del ESP32** (por ejemplo, con *Erase flash*), el `seq` vuelve a 1 y la Pi lo tomaría
  como duplicados: corre la Pi con `--reiniciar` en ese caso.
- `PCD_SetAntennaGain(RxGain_max)` sube la ganancia del RC522 respecto a `nfc_test` para mejorar la lectura a ~2 cm.
