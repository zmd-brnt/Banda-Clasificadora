# Protocolo UART ESP32 ↔ Raspberry Pi (Formato A ampliado)

> **Estado:** acordado por el equipo (Formato A). Implementado en
> `firmware/banda_clasificadora/protocolo.cpp` y `enlace.cpp` (ESP32) y en `rpi/banda/protocolo.py` y
> `supervisor.py` (Pi). Probado con pruebas automáticas y con el ESP32 emulado. **Todavía no se ha probado
> con el ESP32 real conectado a la Raspberry Pi.**
>
> Si cambias una trama, cambia **los dos lados en el mismo commit** y actualiza este documento y las pruebas
> (`firmware/test/test_logica.cpp`, `rpi/tests/test_protocolo.py`).

## 1. Capa física y formato de línea

- UART2 del ESP32 (TX GPIO17, RX GPIO16) ↔ UART de la Raspberry Pi (`/dev/serial0`, pines 8 y 10), GND común.
  Cableado en [conexiones.md](conexiones.md) §4.
- 115200 baudios, 8N1, texto ASCII.
- Una trama por línea: `<cuerpo>*HH\n`
  - `cuerpo`: campos separados por comas, el primero es el tipo.
  - `HH`: XOR de **todos** los caracteres del cuerpo, en hexadecimal de dos dígitos (se aceptan mayúsculas y minúsculas).
  - Ejemplo: `ACK,1*` + XOR('A','C','K',',','1').
- Una trama con checksum incorrecto, sin `*HH`, con campos inválidos o de más de 700 caracteres se **descarta**
  y se cuenta como "trama inválida" (comando `s` en el Monitor Serial; `stats.invalidas` en `GET /api/estado`).
- UID: hexadecimal en MAYÚSCULAS, sin `:`, de 1 a 20 caracteres (las NTAG213 dan 14). `-` = sin UID.

## 2. ESP32 → Raspberry Pi

| Trama | Campos | Cuándo |
|---|---|---|
| `PIEZA,seq,uid,cat,motivo,ciclo_ms,reenvio` | `seq` número de secuencia (≥ 1, se guarda en NVS); `uid` o `-`; `cat` = `A`/`B`/`R`; `motivo` = `OK`/`SIN_LECTURA`/`NO_REGISTRADA`; `ciclo_ms` desde la detección hasta la salida; `reenvio` = `1` si no es el primer intento o si esperó al enlace | Cuando el sensor de salida confirma que la pieza pasó |
| `ESTADO,nombre` | Nombre del estado ([maquina-de-estados.md](maquina-de-estados.md)) | En cada cambio de estado y al reconectar |
| `LATIDO,pendientes,version_tabla,ultimo_seq` | Eventos sin ACK, versión de la tabla local, último `seq` asignado | Cada 1 s, aunque no haya enlace |
| `ALERTA,codigo[,detalle]` | Ver tabla de alertas (§4) | Al ocurrir |
| `CONTENEDORES,cA,cB,cR,capA,capB,capR` | Piezas en cada contenedor desde el último vaciado y su capacidad | Al registrar una pieza, al vaciar, al cambiar capacidad, al arrancar y al reconectar |

`LATIDO` con 3 campos (sin `ultimo_seq`) también se acepta: es el formato del firmware de
`pruebas/integracion_test`.

## 3. Raspberry Pi → ESP32

| Trama | Efecto en el ESP32 |
|---|---|
| `ACK,seq` | Quita ese evento de la cola de pendientes (y de la NVS). Un ACK de algo que ya no está pendiente se ignora. |
| `VIVO,version_tabla` | Respuesta a cada `LATIDO`. Cualquier trama válida mantiene el enlace activo; sin tramas válidas en **3 s** el ESP32 marca el enlace como caído. |
| `SYNC,version,uid=cat;uid=cat;…` | Reemplaza la tabla local (máximo 32 etiquetas) y la guarda en NVS. Si algo es inválido, conserva la anterior y responde `ALERTA,SYNC_INVALIDO`. Si se aplica, responde `ALERTA,SYNC_OK,version`. La lista puede ir vacía: `SYNC,7,` |
| `REENVIAR,seq` | Reenvía ese evento si sigue pendiente; si no, responde `ALERTA,SEQ_NO_DISPONIBLE,seq`. |
| `VACIADO,cat` | Pone en 0 el contenido del contenedor `cat` (y continúa si estaba en `PAUSA_CONTENEDOR`). |
| `CAPACIDAD,cat,n` | Cambia la capacidad del contenedor (1 a 999) y la guarda en NVS. |
| `PARO` | Paro de emergencia **por software** (botón del panel). El paro seguro es el botón físico. |

No existe un comando remoto para **arrancar** o **restablecer**: se decidió que solo se haga con el botón
físico de la banda, para que nadie ponga en marcha la banda sin estar frente a ella.

## 4. Códigos de alerta (`ALERTA,codigo[,detalle]`)

| Código | Detalle | Significado |
|---|---|---|
| `REINICIO` | `ENCENDIDO`, `EXTERNO` (botón EN), `SOFTWARE`, `PANICO`, `WATCHDOG`, `BROWNOUT` (caída de voltaje), `OTRO` | El ESP32 arrancó |
| `ARRANQUE` | — | Se pulsó Restablecer en `PAUSA`: banda en marcha |
| `ATASCO` | `INSPECCION` o `SALIDA` | La pieza no salió de ese sensor en `ATASCO_MS` |
| `SIN_SALIDA` | — | La pieza no llegó al sensor de salida en `SALIDA_TIMEOUT_MS` (el servo no tiene retroalimentación) |
| `PIEZAS_JUNTAS` | — | Llegó otra pieza a inspección antes de terminar la anterior |
| `SALIDA_INESPERADA` | — | Pasó algo por la salida sin haberse detectado en inspección (no se registra) |
| `EMERGENCIA` | `BOTON`, `PANEL`, `MONITOR` | Paro de emergencia |
| `RESTABLECIDO` | — | Se salió de `ERROR` o `EMERGENCIA` con el botón Restablecer |
| `CONTENEDOR_LLENO` | `A`/`B`/`R` | Banda en pausa hasta vaciar ese contenedor |
| `VACIADO` | `A`/`B`/`R` | Vaciado confirmado con el botón de la banda (sin la Pi) |
| `NFC_FALLA` | — | El RC522 no responde al arrancar: todo va a rechazo `SIN_LECTURA` |
| `PENDIENTES_PERDIDOS` | `seq` | La cola de 16 pendientes se llenó y se descartó el evento más antiguo |
| `SEQ_NO_DISPONIBLE` | `seq` | Respuesta a `REENVIAR` de un evento que ya no está |
| `SYNC_OK` | versión | Tabla aplicada |
| `SYNC_INVALIDO` | — | Tabla rechazada |

La Raspberry Pi agrega sus propias alertas (no viajan por la UART): `ENLACE_OK`, `ENLACE_CAIDO`,
`UART_HUECO`, `NUMERACION_REINICIADA`, `PARO_PANEL`.

## 5. Reglas de recuperación

1. **ACK y reintentos.** Cada `PIEZA` queda en una cola de 16 eventos (guardada en NVS). Mientras haya enlace se
   reintenta cada 1 s hasta recibir `ACK`. Sin enlace se conserva y se envía al reconectar con `reenvio = 1`.
2. **Duplicados.** La Pi guarda cada pieza con clave única `(epoca, seq)`. Un duplicado se descarta, pero se
   responde `ACK` igual para que el ESP32 deje de reenviarlo.
3. **Huecos.** Si llega un `seq` mayor que el máximo + 1, la Pi pide `REENVIAR` de los que faltan (si el
   historial no está vacío). Si el ESP32 ya no los tiene, responde `SEQ_NO_DISPONIBLE`.
4. **Tabla maestra.** La Pi compara la versión del `LATIDO` con la suya; si difieren, envía `SYNC` (como mucho
   una vez cada 2 s). Al registrar o eliminar una etiqueta en el panel, la Pi envía `SYNC` de inmediato.
5. **Memoria borrada.** Si el `LATIDO` trae un `ultimo_seq` menor que el mayor `seq` registrado, el ESP32
   perdió su NVS: la Pi abre una **época** nueva y la numeración puede empezar en 1 sin confundirse con duplicados.
6. **Hora.** El ESP32 no tiene reloj: la Pi pone la fecha y hora al **recibir**. Para piezas que llegan tarde
   (`tardio` en la base de datos) esa hora es la de recepción, no la de clasificación.

## 6. Límites conocidos

- Máximo 32 etiquetas en la tabla (memoria del ESP32) y 700 caracteres por trama: la Pi rechaza registrar
  una etiqueta que no quepa.
- Si el ESP32 pasa más de 16 piezas sin enlace, se pierden las más antiguas (`PENDIENTES_PERDIDOS`).
- No hay cifrado ni autenticación: el enlace es un cable directo entre los dos controladores.
