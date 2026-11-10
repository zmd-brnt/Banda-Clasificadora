# Máquina de estados del ESP32

> **Estado:** implementada en `firmware/banda_clasificadora/banda_clasificadora.ino` (`cicloClasificacion()`),
> compilada, **sin probar en hardware**. Los tiempos marcados como [PROVISIONAL] en `config.h` no están medidos.

## 1. Diagrama

```
                 encendido
                     │
                     ▼
               ┌──────────┐  Restablecer
               │  PAUSA   │──────────────┐
               └──────────┘              ▼
                                   ┌──────────┐ sensor de inspección detecta la pieza
      ┌───────────────────────────►│  ESPERA  │──────────────────────┐ (motor se detiene)
      │                            └──────────┘                      ▼
      │                                                     ┌──────────────┐
      │                                                     │ LECTURA_NFC  │ hasta 1.5 s
      │                                                     └──────┬───────┘
      │                               contenedor destino lleno     │ UID → tabla local → A / B / R
      │                         ┌──────────────────────────────────┤
      │                         ▼                                  ▼
      │               ┌──────────────────┐  vaciado      ┌──────────────┐
      │               │ PAUSA_CONTENEDOR │──────────────►│ CLASIFICANDO │ servo a su posición (SERVO_MS)
      │               └──────────────────┘               └──────┬───────┘
      │                                                         │ motor en marcha
      │                                                         ▼
      │                                               ┌───────────────────┐  > ATASCO_MS
      │                                               │ LIBERA_INSPECCION │───────────────► ERROR (ATASCO)
      │                                               └─────────┬─────────┘
      │                                                         │ inspección libre
      │                                                         ▼
      │                                               ┌───────────────────┐  sin llegar > SALIDA_TIMEOUT_MS
      │                                               │  CONFIRMA_SALIDA  │──────────────► ERROR (SIN_SALIDA)
      │                                               └─────────┬─────────┘  bloqueado > ATASCO_MS
      │                                                         │            ──────────────► ERROR (ATASCO)
      │                                                         │ la pieza pasa el sensor de salida
      │                                                         │ → conteo + PIEZA por UART
      │                                                         ▼
      │                 RETORNO_CENTRO_MS                ┌──────────┐
      └──────────────────────────────────────────────────│ RETORNO  │ compuerta al centro
                                                         └──────────┘

  Desde CUALQUIER estado: botón de paro (o PARO del panel, o 'e') ──► EMERGENCIA
  ERROR / EMERGENCIA ──(paro suelto + sensores libres + Restablecer)──► ESPERA
```

## 2. Tabla de estados

| Estado | Motor | Compuerta | LEDs | Sale cuando… |
|---|---|---|---|---|
| `PAUSA` | detenido | centro | amarillo | Se pulsa **Restablecer** y el paro no está presionado |
| `ESPERA` | en marcha | centro | verde | El sensor de inspección detecta una pieza (filtrado) |
| `LECTURA_NFC` | detenido | centro | verde | Se lee el UID o pasan 1.5 s |
| `PAUSA_CONTENEDOR` | detenido | centro | amarillo, 2 pitidos | `VACIADO` desde el panel o **Restablecer** (= confirmar vaciado) |
| `CLASIFICANDO` | detenido | A / B / centro | verde | Pasan `SERVO_MS` |
| `LIBERA_INSPECCION` | en marcha | posición elegida | verde | La pieza deja el sensor de inspección |
| `CONFIRMA_SALIDA` | en marcha | posición elegida | verde | La pieza aparece y luego deja el sensor de salida |
| `RETORNO` | en marcha | posición elegida | verde | Pasan `RETORNO_CENTRO_MS` → compuerta al centro |
| `ERROR` | detenido | centro | rojo, buzzer intermitente | **Restablecer** con los dos sensores libres y el paro suelto |
| `EMERGENCIA` | detenido (por hardware si fue el botón) | centro | rojo, buzzer intermitente | Igual que `ERROR` |

## 3. Decisiones de diseño (y por qué)

1. **Arranque en `PAUSA`.** Después de un reinicio (por ejemplo una caída de voltaje), la banda no se mueve
   sola: hay que pulsar Restablecer. Así nadie se sorprende con la banda arrancando y la pieza que estaba a la
   mitad no se clasifica a ciegas.
2. **La pieza se registra al confirmar la salida**, no al leerla. Si la pieza no llega al sensor de salida, no
   se cuenta y se entra en `ERROR SIN_SALIDA`: como el servo no informa su posición, el sensor de salida es la
   única confirmación indirecta (avance §11).
3. **Emergencia: la pieza en curso no se registra.** Su destino es incierto.
4. **Piezas juntas:** se da aviso (`PIEZAS_JUNTAS`) y la segunda pieza se atiende al volver a `ESPERA`. Como la
   banda sigue en marcha mientras la primera sale, la segunda puede quedar fuera de la zona de lectura: en el
   peor caso irá a **rechazo `SIN_LECTURA`**, nunca a una categoría equivocada. Si pasa seguido, hay que
   separar más las piezas.
5. **Contenedor lleno se revisa antes de mover la compuerta**, con la pieza todavía detenida en inspección.
6. **Restablecer y arranque solo con el botón físico** (o `r` por el Monitor Serial en pruebas de mesa). El panel
   solo puede pedir un **paro** por software.

## 4. Suposición sobre la geometría (confirmar con el módulo A)

El firmware asume este orden a lo largo de la banda, que es el mismo del simulador del panel:

```
entrada ── sensor de inspección + RC522 ── … ── sensor de salida ── fin de banda + compuerta ── contenedores
```

Es decir: **todas** las piezas pasan por el sensor de salida antes de llegar a la compuerta, y la compuerta las
desvía al caer. Si el montaje real es distinto (por ejemplo, la compuerta desvía antes del sensor de salida),
hay que cambiar la lógica de `CONFIRMA_SALIDA`.

## 5. Tiempos y umbrales

Todos en `firmware/banda_clasificadora/config.h`. Los marcados [PROVISIONAL] **no están medidos**:

| Constante | Valor | Origen |
|---|---|---|
| `LECTURA_NFC_MS` | 1500 ms | Documento de avance §5 |
| `SERVO_MS` | 400 ms | `pruebas/integracion_test` |
| `ATASCO_MS` | 5000 ms | [PROVISIONAL] ("N segundos" del avance) |
| `SALIDA_TIMEOUT_MS` | 6000 ms | [PROVISIONAL]: depende de la distancia inspección → salida y de la velocidad |
| `RETORNO_CENTRO_MS` | 1000 ms | [PROVISIONAL]: tiempo para que la pieza caiga tras pasar la salida |
| `RETARDO_PARO_MS` | 0 ms | [PROVISIONAL]: para centrar la etiqueta sobre el RC522 |
| `PWM_BANDA_PCT` | 60 % | [PROVISIONAL]: medir con `pruebas/motor_test` |
| `UMBRAL_INSP_CM`, `UMBRAL_SALIDA_CM` | 8 cm | [PROVISIONAL]: medir con `pruebas/ultrasonico_test` (modo `c`) |
| `CAPACIDAD_INICIAL` | 15 piezas | Decisión del equipo; se cambia desde el panel |
