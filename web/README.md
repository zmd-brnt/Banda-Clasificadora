# Panel web — Banda Clasificadora NFC

Prototipo **visual** del panel de supervisión que correrá en la Raspberry Pi 3 Model B.
Por ahora **no hay hardware ni comunicación UART conectados**: todos los datos los genera un
**simulador del ESP32** dentro de la propia página. El objetivo es acordar cómo se verá el
panel y cómo se organizarán los datos antes de escribir el servidor.

> **Estado:** plantilla visual + simulador → **hecho**.
> Servidor en la Raspberry Pi, lectura UART real y base de datos → **pendiente**.

---

## 1. Cómo abrirlo

No necesita instalar nada ni conexión a internet (no usa librerías externas).

```bash
# Opción A: doble clic en web/index.html

# Opción B: servidor local (igual a como funcionará en la Raspberry Pi)
cd web
python3 -m http.server 8000
# abrir http://localhost:8000
```

Después presiona **▶ Iniciar simulación**.

---

## 2. Apartados del panel

| Pestaña | Para qué sirve | Qué muestra |
|---|---|---|
| **Tiempo real** | Ver lo que está pasando *ahora* en la banda | Indicadores (total, A, B, rechazo, tiempo de ciclo), animación de la banda con sensores, lector, compuerta y contenedores, estado de la máquina de estados, barra de distribución y **feed de eventos en vivo** |
| **Inventario** | Ver el acumulado por categoría y por objeto | Tarjeta por contenedor (total, ocupación vs. capacidad, *Marcar vaciado*) y tabla por UID con nombre, categoría, cantidad y última lectura, con filtro y búsqueda |
| **Etiquetas** | Administrar la **tabla maestra** | Formulario para registrar UID → nombre → categoría, lista de UID detectados sin registrar (registro en un clic), tabla de etiquetas y botón **Sincronizar con ESP32** con aviso de versión desactualizada |
| **Historial** | Consultar cada pieza procesada | Tabla por número de secuencia: hora, UID, objeto, destino, motivo de rechazo, tiempo de ciclo y si llegó en vivo o por reenvío. **Exportar CSV** (útil para las evidencias de las 50 pruebas) |
| **Alertas** | Diagnóstico | Lista de alertas con nivel (crítico / advertencia / info), código y descripción. Botón *Reconocer todas* |

En el encabezado siempre se ven tres indicadores: **modo** (simulación o datos reales),
**enlace UART** (latido del ESP32 y eventos pendientes) y **estado del ESP32**.

---

## 3. Simulación

La barra superior controla un ESP32 virtual que sigue el flujo del documento de avance:

1. **Transporte** → la pieza avanza hasta el IR de inspección.
2. **Lectura NFC** → la banda se detiene; el RC522 tiene **1.5 s** para leer el UID.
3. **Consulta de la tabla local** → categoría A, B, o rechazo (sin lectura / no registrada).
4. **Compuerta** → servo a izquierda (A), derecha (B) o centro (rechazo).
5. **Confirma salida** → la banda reanuda y el IR de salida confirma el paso.
6. Se actualizan los contadores y se envía el evento a la Raspberry Pi con **número de secuencia**.

| Control | Qué hace |
|---|---|
| ▶ Iniciar / ⏸ Pausar | Enciende el ESP32 virtual; la pausa ocurre al terminar la pieza en curso |
| ⏹ Paro de emergencia | Detiene motor, compuerta al centro, estado `EMERGENCIA` |
| ↺ Restablecer | Equivale al botón físico de reinicio tras error o paro |
| Velocidad | 0.5× a 4× (para no esperar en las demostraciones) |
| Limpiar datos | Borra conteos, historial y alertas |

**Inyectar falla** reproduce los casos de la tabla de manejo de errores del documento:

| Falla | Respuesta esperada que se ve en el panel |
|---|---|
| Pieza sin etiqueta | Espera 1.5 s → rechazo, contador "sin leer" |
| Etiqueta no registrada | Rechazo; el UID aparece en *Etiquetas → UID detectados sin registrar* |
| Atasco | Motor detenido, estado `ERROR`, alerta crítica; requiere *Restablecer* |
| Enlace UART caído (10 s) | El panel marca "Sin latido"; el ESP32 **sigue clasificando** y guarda los eventos; al reconectar llegan marcados como *reenviados* |
| Mensaje UART duplicado | El panel lo detecta por `seq` repetido y lo descarta |
| Mensaje UART perdido | El panel detecta el hueco en `seq`, pide reenvío y lo inserta en orden |
| Contenedor A lleno | Banda en pausa hasta pulsar *Marcar vaciado* en Inventario |
| Piezas muy juntas | Alerta de advertencia |

**Prueba sugerida del flujo de registro:** espera a que aparezca un UID no registrado →
regístralo en *Etiquetas* → observa que el aviso dice *desactualizada* y que esa pieza
**sigue yendo a rechazo** → pulsa *Sincronizar con ESP32* → a partir de ahí se clasifica bien.
Esto refleja la arquitectura: el ESP32 clasifica con **su copia local** de la tabla.

---

## 4. Estructura de archivos

```
web/
├── index.html          # Estructura de las 5 vistas
├── css/estilos.css     # Estilos, tema claro/oscuro, responsivo
└── js/
    ├── estado.js       # Modelo de datos del panel (lo que guardará la Raspberry Pi)
    ├── ui.js           # Dibuja el panel a partir del modelo; animación de la banda
    ├── simulador.js    # ESP32 virtual (se elimina cuando haya hardware)
    └── main.js         # Conecta botones, pestañas y la fuente de datos
```

**Idea clave del diseño:** todos los datos entran por **una sola función**,
`Panel.recibir(mensaje)`, y todos los comandos salen por `Panel.enviar(comando)`.
Hoy ambas están conectadas al simulador; para usar el hardware solo se cambia esa conexión
en `main.js`, y el resto del panel queda igual.

---

## 5. Organización de los datos

### 5.1 Mensajes ESP32 → Raspberry Pi (ya decodificados a JSON)

| Tipo | Campos | Cuándo se envía |
|---|---|---|
| `PIEZA` | `seq`, `ts`, `uid` (o `null`), `categoria` (`A`/`B`/`R`), `motivo` (`null`/`SIN_LECTURA`/`NO_REGISTRADA`), `ciclo_ms`, `reenvio` | Al confirmar la salida de cada pieza |
| `ESTADO` | `ts`, `estado`, `detalle` | En cada cambio de la máquina de estados |
| `LATIDO` | `ts`, `pendientes`, `version_tabla` | Cada 1 s |
| `ALERTA` | `ts`, `nivel`, `codigo`, `mensaje` | Atasco, emergencia, contenedor lleno, etc. |

Estados de la máquina: `ESPERA`, `TRANSPORTE`, `LECTURA_NFC`, `CLASIFICANDO`,
`CONFIRMA_SALIDA`, `PAUSA`, `PAUSA_CONTENEDOR`, `ERROR`, `EMERGENCIA`.

Ejemplo de pieza:

```json
{ "tipo": "PIEZA", "seq": 42, "ts": 1761674400000, "uid": "04:A3:1F:B2:6C:80:00",
  "categoria": "A", "motivo": null, "ciclo_ms": 4630 }
```

### 5.2 Comandos Raspberry Pi → ESP32

| Comando | Campos | Para qué |
|---|---|---|
| `ACK` | `seq` | Confirma recepción; el ESP32 deja de guardar ese evento |
| `REENVIAR` | `seqs[]` | Pide los mensajes que faltan |
| `SYNC` | `version`, `tabla[]` | Envía la tabla maestra al ESP32 |
| `VACIADO` | `categoria` | Reinicia el contador de ocupación de un contenedor |

### 5.3 Datos que guarda el panel (futuras tablas SQLite)

| Tabla | Columnas |
|---|---|
| `etiquetas` | `uid` (PK), `nombre`, `categoria`, `registrada` |
| `piezas` | `seq` (PK), `ts`, `uid`, `categoria`, `motivo`, `ciclo_ms`, `reenvio`, `recibido` |
| `alertas` | `id`, `ts`, `nivel`, `codigo`, `mensaje`, `reconocida` |
| `contenedores` | `categoria` (PK), `capacidad`, `contenido` |

El **inventario** y los **conteos** no necesitan tabla propia: se calculan con consultas sobre
`piezas` (por ejemplo `SELECT categoria, COUNT(*) FROM piezas GROUP BY categoria`).

---

## 6. Qué es real y qué es supuesto

| Elemento | Origen |
|---|---|
| Flujo de 8 pasos, límite de 1.5 s, categorías A/B/rechazo, servo izquierda/centro/derecha | Documento de avance |
| Tabla de errores (sin etiqueta, no registrada, atasco, enlace caído, duplicados, contenedor lleno, emergencia) | Documento de avance |
| UID, categoría y número de secuencia en cada evento | Documento de avance |
| Formato JSON, nombres de campos, comandos `ACK`/`REENVIAR`/`SYNC`/`VACIADO` | **Propuesta de este prototipo**; hay que acordarla |
| Tiempos de la simulación (1.6 s de transporte, 1.2 s a la salida, etc.) | **Inventados** para que se vea fluido; se deben reemplazar con mediciones reales |
| Capacidad de 15 piezas por contenedor, 7 % de piezas sin etiqueta | **Inventados** |
| UIDs y nombres de objetos de ejemplo | **Inventados** |

Ningún valor mostrado por el simulador es un resultado experimental.

---

## 7. Mejoras por implementar

### Siguientes pasos (para conectar el hardware)

1. **Definir el formato de trama UART.** Propuesta compacta y fácil de validar en el ESP32:
   ```
   $PZ,42,04A31FB26C8000,A,-,4630*CS\n     pieza   (seq, uid, cat, motivo, ciclo_ms)
   $ST,LECTURA_NFC*CS\n                   estado
   $HB,0,3*CS\n                           latido  (pendientes, versión de tabla)
   $AL,C,ATASCO*CS\n                      alerta
   $ACK,42*CS\n                           confirmación (Raspberry → ESP32)
   ```
   `*CS` es un checksum de 2 dígitos hex (XOR de los bytes entre `$` y `*`, como en NMEA) para descartar tramas corruptas.
   Partir de `pruebas/uart_test/rpi/rpi_uart_test.py`, que ya abre `/dev/serial0`.
2. **Servidor en la Raspberry Pi** (recomendado: Python + Flask, ligero para la Pi 3):
   - Un hilo lee la UART, valida checksum, convierte a JSON y guarda en SQLite.
   - `GET /api/estado` → conteos, contenedores, tabla maestra, últimas piezas (para cargar la página).
   - `GET /api/eventos` → **Server-Sent Events** con cada mensaje nuevo (más simple que WebSockets).
   - `POST /api/comando` → `SYNC`, `VACIADO`, etc. hacia la UART.
3. **Cambiar la fuente de datos en `main.js`:**
   ```js
   const fuente = new EventSource("/api/eventos");
   fuente.onmessage = (e) => Panel.recibir(JSON.parse(e.data));
   Panel.fuente = "uart";
   Panel.enviar = (cmd) => fetch("/api/comando", { method: "POST",
     headers: { "Content-Type": "application/json" }, body: JSON.stringify(cmd) });
   ```
   y quitar `simulador.js` del HTML (o dejarlo detrás de un parámetro `?demo`).
4. **Hora de los eventos.** El ESP32 no tiene reloj de tiempo real: debe enviar `millis()` y
   la Raspberry Pi asignar la fecha y hora al recibir. Para eventos reenviados tras una
   desconexión, calcular la hora con la diferencia de `millis()` respecto al último latido.
5. **Persistencia.** Hoy todo vive en la memoria del navegador y se pierde al recargar.
   Con SQLite en la Raspberry Pi el historial sobrevive reinicios.
6. **Sincronizar al arrancar.** Al iniciar, la Raspberry Pi compara su `version_tabla` con la del
   latido del ESP32 y envía `SYNC` automáticamente si difieren.

### Mejoras de funcionalidad

- Recuperación tras reinicio del ESP32: que reporte en el latido sus contadores guardados en NVS y el último `seq`, para que el panel no confunda un reinicio (`seq` vuelve a 1) con duplicados.
- Editar categoría de una etiqueta existente (hoy hay que eliminar y volver a registrar).
- Registrar etiquetas acercándolas al lector (modo "aprender UID") en lugar de escribirlas a mano.
- Configurar desde el panel la capacidad de cada contenedor y los nombres de las categorías.
- Gráfica de piezas por minuto y de tiempo de ciclo a lo largo del tiempo.
- Resumen automático de las pruebas de sistema: aciertos, rechazos, lecturas fallidas y tiempo de ciclo promedio sobre un rango de `seq`.
- Usuario/contraseña sencillos para las acciones que modifican la tabla maestra.

### Limitaciones conocidas del prototipo

- La animación de la banda solo existe en simulación; con datos reales se puede animar a partir de los mensajes `ESTADO`, pero no mostrará la posición exacta de la pieza.
- La inyección "Contenedor A lleno" modifica directamente el contador del panel y del ESP32 virtual (atajo de simulación).
- La pausa se aplica al terminar la pieza en curso, no a mitad del recorrido.
