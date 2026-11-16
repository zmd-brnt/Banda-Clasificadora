# Panel web — Banda Clasificadora NFC

Panel de supervisión que sirve la Raspberry Pi 3 Model B. Funciona de dos formas:

- **Modo UART (datos reales):** la página la sirve `rpi/servidor.py`. Los datos vienen del ESP32 por la UART y
  se guardan en SQLite en la Pi; el panel los recibe en vivo (Server-Sent Events).
- **Modo simulación:** si la página se abre sin ese servidor (doble clic, `python3 -m http.server`) o con
  `?demo` en la URL, todos los datos los genera un **simulador del ESP32** dentro de la propia página.

> **Estado:** ambos modos probados en un navegador simulado (jsdom) con el ESP32 emulado
> (`rpi/emulador_esp32.py`). **Todavía no se ha probado con el ESP32 real ni en la Raspberry Pi.**

---

## 1. Cómo abrirlo

No necesita instalar nada ni conexión a internet (no usa librerías externas).

**Con datos reales** (en la Raspberry Pi, ver [rpi/README.md](../rpi/README.md)):
```bash
cd rpi
python3 servidor.py           # abrir http://<ip-de-la-pi>:8080
```
El encabezado muestra **Datos reales (UART)**. Se ocultan los controles de simulación; queda el botón de paro
(paro **por software**: el seguro es el botón físico). Arrancar y restablecer solo se hacen con el botón
Restablecer de la banda.

**Simulación:**
```bash
# Opción A: doble clic en web/index.html
# Opción B:
cd web
python3 -m http.server 8000   # abrir http://localhost:8000
```
Después presiona **▶ Iniciar simulación**.

---

## 2. Apartados del panel

| Pestaña | Para qué sirve | Qué muestra |
|---|---|---|
| **Tiempo real** | Ver lo que está pasando *ahora* en la banda | Resumen (total, A, B, rechazo, tiempo de ciclo), estado de la máquina de estados como línea de pasos con la lectura actual, y **feed de eventos en vivo** |
| **Inventario** | Ver el acumulado por categoría y por objeto | Tarjeta por contenedor (total, ocupación vs. capacidad, *Marcar vaciado*) y tabla por UID con nombre, categoría, cantidad y última lectura, con filtro y búsqueda |
| **Etiquetas** | Administrar la **tabla maestra** | Formulario para registrar UID → nombre → categoría, lista de UID detectados sin registrar (registro en un clic), tabla de etiquetas y botón **Sincronizar con ESP32** con aviso de versión desactualizada |
| **Historial** | Consultar cada pieza procesada | Tabla por número de secuencia: hora, UID, objeto, destino, motivo de rechazo, tiempo de ciclo y si llegó en vivo o por reenvío. **Exportar CSV** (útil para las evidencias de las 50 pruebas) |
| **Alertas** | Diagnóstico | Lista de alertas con nivel (crítico / advertencia / info), código y descripción. Botón *Reconocer todas* |

En el encabezado siempre se ven tres indicadores: **modo** (simulación o datos reales),
**enlace UART** (latido del ESP32 y eventos pendientes) y **estado del ESP32**.

---

## 3. Simulación

La barra superior controla un ESP32 virtual que sigue el flujo del documento de avance:

1. **Transporte** → la pieza avanza hasta el sensor de inspección (HC-SR04).
2. **Lectura NFC** → la banda se detiene; el RC522 tiene **1.5 s** para leer el UID.
3. **Consulta de la tabla local** → categoría A, B, o rechazo (sin lectura / no registrada).
4. **Compuerta** → servo a izquierda (A), derecha (B) o centro (rechazo).
5. **Confirma salida** → la banda reanuda y el sensor de salida (HC-SR04) confirma el paso.
6. Se actualizan los contadores y se envía el evento a la Raspberry Pi con **número de secuencia**.

| Control | Qué hace |
|---|---|
| ▶ Iniciar / Reanudar / ⏸ Pausar | Enciende el ESP32 virtual; la pausa ocurre al terminar la pieza en curso |
| ⏹ Detener (paro de emergencia) | Detiene motor, compuerta al centro, estado `EMERGENCIA` |
| Restablecer | Equivale al botón físico de reinicio tras error o paro |
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
├── css/estilos.css     # Estilos: barra lateral, tema oscuro Medianoche (claro con el botón ◐), responsivo
└── js/
    ├── estado.js       # Modelo de datos del panel (lo que guardará la Raspberry Pi)
    ├── ui.js           # Dibuja el panel a partir del modelo
    ├── simulador.js    # ESP32 virtual (solo se usa en modo simulación)
    └── main.js         # Conecta botones, pestañas y la fuente de datos
```

**Idea clave del diseño:** todos los datos entran por **una sola función**,
`Panel.recibir(mensaje)`, y todos los comandos salen por `Panel.enviar(comando)`.
En modo simulación están conectadas al simulador; en modo UART `main.js` (`modoUart()`) las conecta a
la API de la Pi (`api/eventos`, `api/comando`, `api/etiquetas`). El resto del panel es el mismo en ambos modos.

---

## 5. Organización de los datos

### 5.1 Mensajes ESP32 → Raspberry Pi (ya decodificados a JSON)

| Tipo | Campos | Cuándo se envía |
|---|---|---|
| `PIEZA` | `seq`, `ts`, `uid` (o `null`), `categoria` (`A`/`B`/`R`), `motivo` (`null`/`SIN_LECTURA`/`NO_REGISTRADA`), `ciclo_ms`, `reenvio` | Al confirmar la salida de cada pieza |
| `ESTADO` | `ts`, `estado`, `detalle` | En cada cambio de la máquina de estados |
| `LATIDO` | `ts`, `pendientes`, `version_tabla` | Cada 1 s |
| `ALERTA` | `ts`, `nivel`, `codigo`, `mensaje` | Atasco, emergencia, contenedor lleno, etc. |
| `CONTENEDORES` | ocupación y capacidad de A, B y R | Modo UART: cuando el ESP32 los reporta |
| `ETIQUETAS` | tabla maestra y versión | Modo UART: al registrar o eliminar una etiqueta |
| `ENLACE` | `ok` | Modo UART: la Pi detecta que el ESP32 dejó de (o volvió a) enviar latidos |

Estos son los mensajes **ya convertidos a JSON** que recibe `Panel.recibir()`. Las tramas que viajan por la
UART están en [docs/protocolo-uart.md](../docs/protocolo-uart.md).

Estados de la máquina ([docs/maquina-de-estados.md](../docs/maquina-de-estados.md)): `PAUSA`, `ESPERA`,
`LECTURA_NFC`, `CLASIFICANDO`, `LIBERA_INSPECCION`, `CONFIRMA_SALIDA`, `RETORNO`, `PAUSA_CONTENEDOR`, `ERROR`,
`EMERGENCIA`. El simulador usa además `TRANSPORTE` (la pieza viaja hacia el sensor).

Ejemplo de pieza (en modo UART el panel usa además `epoca`, ver `docs/protocolo-uart.md` §5):

```json
{ "tipo": "PIEZA", "seq": 42, "ts": 1761674400000, "uid": "04A31FB26C8000",
  "categoria": "A", "motivo": null, "ciclo_ms": 4630 }
```

### 5.2 Comandos Raspberry Pi → ESP32

| Comando | Campos | Para qué |
|---|---|---|
| `ACK` | `seq` | Confirma recepción; el ESP32 deja de guardar ese evento |
| `REENVIAR` | `seqs[]` | Pide los mensajes que faltan |
| `SYNC` | `version`, `tabla[]` | Envía la tabla maestra al ESP32 |
| `VACIADO` | `categoria` | Reinicia el contador de ocupación de un contenedor |
| `CAPACIDAD` | `categoria`, `capacidad` (1–999) | Cambia la capacidad de un contenedor (Inventario → campo de capacidad) |
| `PARO` | — | Paro de emergencia por software |

En modo UART, `ACK` y `REENVIAR` los resuelve el servidor de la Pi, no el navegador.

### 5.3 Datos que guarda la Raspberry Pi (SQLite, `rpi/banda/db.py`)

| Tabla | Columnas principales |
|---|---|
| `etiquetas` | `uid` (PK), `nombre`, `categoria` |
| `piezas` | `epoca` + `seq` (únicos), hora de recepción, `uid`, `categoria`, `motivo`, `ciclo_ms`, `reenvio`, `tardio` |
| `alertas` | hora, `nivel`, `codigo`, `mensaje`, `reconocida` |
| `contenedores` | `categoria` (PK), `capacidad`, `contenido` |
| `config` | `version_tabla`, `epoca` |

En modo simulación todo vive en la memoria del navegador y se pierde al recargar.

El **inventario** y los **conteos** no necesitan tabla propia: se calculan con consultas sobre
`piezas` (por ejemplo `SELECT categoria, COUNT(*) FROM piezas GROUP BY categoria`).

---

## 6. Qué es real y qué es supuesto

| Elemento | Origen |
|---|---|
| Flujo de 8 pasos, límite de 1.5 s, categorías A/B/rechazo, servo izquierda/centro/derecha | Documento de avance |
| Tabla de errores (sin etiqueta, no registrada, atasco, enlace caído, duplicados, contenedor lleno, emergencia) | Documento de avance |
| UID, categoría y número de secuencia en cada evento | Documento de avance |
| Tramas UART | Formato A ampliado, acordado por el equipo (`docs/protocolo-uart.md`) |
| Formato JSON interno del panel | Diseño de este panel (no viaja por la UART) |
| Tiempos de la simulación (1.6 s de transporte, 1.2 s a la salida, etc.) | **Inventados** para que se vea fluido; se deben reemplazar con mediciones reales |
| Capacidad inicial de 15 piezas por contenedor | Decisión del equipo; configurable desde el panel |
| 7 % de piezas sin etiqueta | **Inventado** |
| UIDs y nombres de objetos de ejemplo (simulador y `rpi/emulador_esp32.py`) | **Inventados** |

Ningún valor mostrado por el simulador es un resultado experimental.

---

## 7. Mejoras por implementar

Ya hechas en esta etapa: formato de trama UART, servidor en la Pi con SQLite y SSE, conexión del panel a datos
reales, hora asignada por la Pi, SYNC automático, manejo de reinicio del ESP32 (épocas de `seq`) y capacidad
configurable por contenedor.

### Mejoras de funcionalidad (pendientes)

- Editar categoría de una etiqueta existente: hoy se registra de nuevo el mismo UID con otra categoría (la Pi la reemplaza).
- Registrar etiquetas acercándolas al lector (modo "aprender UID") en lugar de escribirlas a mano.
- Nombres de las categorías configurables.
- Gráfica de piezas por minuto y de tiempo de ciclo a lo largo del tiempo.
- Resumen automático de las pruebas de sistema: aciertos, rechazos, lecturas fallidas y tiempo de ciclo promedio sobre un rango de `seq`.
- Usuario/contraseña sencillos para las acciones que modifican la tabla maestra (hoy cualquiera en la red puede usar el panel).

### Limitaciones conocidas

- Para piezas que llegan tarde (tras una desconexión) la hora es la de recepción en la Pi, no la de clasificación.
- En simulación, la inyección "Contenedor A lleno" modifica directamente el contador del panel y del ESP32 virtual (atajo de simulación); la pausa se aplica al terminar la pieza en curso.
