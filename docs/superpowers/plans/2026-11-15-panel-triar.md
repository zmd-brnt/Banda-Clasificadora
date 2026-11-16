# Panel Triar (`web2/`) — plan de implementación

> **Para quien lo ejecute:** USAR `superpowers:subagent-driven-development` (recomendado) o `superpowers:executing-plans` para llevar el plan tarea por tarea. Los pasos usan casillas `- [ ]`.
> **Commits:** no se hace ningún commit sin que el usuario lo pida. Cada tarea termina con la lista de archivos para `git add`.

**Objetivo:** construir el panel web nuevo "Triar" en `web2/`: supervisión y operación de la banda en laptop, supervisión en teléfono, con modo demo propio.

**Arquitectura:** HTML + CSS + JS puros, sin compilar ni instalar nada. Cada archivo JS es un script clásico (funciona con doble clic en `file://` para el modo demo) que cuelga de un espacio global `Triar` y, si existe `module`, también se exporta, así `node --test` lo prueba sin dependencias. La lógica (modelo, formato, fuentes de datos, modo demo) está separada de las vistas, que son funciones puras que devuelven HTML; `app.js` es lo único que toca el DOM. El servidor de la Pi no cambia: `python3 servidor.py --web ../web2`.

**Tecnologías:** JS (ES2020), CSS, Node ≥ 20 solo para las pruebas (`node:test`, ya instalado: v24), Firefox headless para capturas.

**Spec:** `docs/superpowers/specs/2026-11-15-panel-nuevo-design.md` · marca: `docs/marca/README.md` · contrato de datos: `rpi/README.md` §4 y `web/README.md` §5 (solo lectura; **no se reutiliza código de `web/`**).

## Restricciones globales

(Valores copiados del spec; cada tarea las incluye.)

- Sin internet: **ninguna** fuente, librería, CDN ni petición externa. Todo local.
- Colores (oscuro / claro): fondo `#0E1417`/`#F2F5F6`; superficie `#172127`/`#FFFFFF`; texto `#E8EEF0`/`#0E1417`; texto-2 `#9AA9B1`/`#4B5A62`; texto-3 `#8A9AA3`/`#5A6770`; acento `#38D6C8`/`#00766E` (texto sobre acento `#0E1417`/`#FFFFFF`); A `#5AA9FF`/`#1F67C9`; B `#FF7AB6`/`#B8185F`; R `#9AA9B1`/`#5A6770`; OK `#4ADE80`/`#167A32`; aviso `#FBBF24`/`#8F5400`; crítico `#FF6B5E`/`#C0342A`. Todo texto ≥ 4.5:1.
- Colores de categoría (A/B/R) nunca se usan para estados, y al revés. Ningún estado se comunica solo con color.
- Tipografía: `system-ui, -apple-system, "Segoe UI", Roboto, sans-serif`; datos en `ui-monospace, "SF Mono", Menlo, Consolas, monospace`. Cuerpo 17 px; nada < 12 px; escala 12/14/17/22/34/48 px; todo en `rem`.
- Pantalla ancha: ≥ 900 px = supervisión + controles; < 900 px = **solo supervisión**. Teléfono: 4 pestañas (Tiempo real, Inventario, Historial, Alertas); sin Etiquetas.
- Objetivos táctiles ≥ 44 px (teléfono) y ≥ 28 px (laptop); foco de teclado visible; cada icono con texto o `aria-label`.
- `PARO` de un toque **sin confirmación**, solo pantalla ancha, con la leyenda "Paro por software; el seguro es el botón físico". Arrancar y restablecer no existen en el panel real.
- Textos en español, mayúscula solo en la primera palabra, botones con verbo ("Marcar vaciado", "Sincronizar con ESP32").
- Modo demo: franja visible "Modo demostración — datos inventados"; ningún valor demo es resultado experimental.
- Nombre: **Triar**, subtítulo "Banda Clasificadora"; logo de `docs/marca/`.

**Ajustes al spec decididos aquí (mi criterio):**
1. Orden de la banda según `docs/maquina-de-estados.md` §4: entrada → sensor de inspección → lector RC522 → sensor de salida → compuerta → contenedores (el spec §6.3 omitía el sensor de salida).
2. `?tema=oscuro|claro` fuerza el tema (útil para proyectar); sin él se sigue el modo del sistema. No hay interruptor visible.
3. La compuerta no muestra destino mientras se clasifica (el estado del ESP32 no lo informa); el contenedor destino se resalta 2.5 s cuando llega la `PIEZA`.

## Puntos de revisión (casos que el spec no cubre y que pueden fallar)

1. Un nombre de etiqueta con HTML (`<img src=x onerror=alert(1)>`) debe verse como texto en **todas** las vistas (cada pieza lo escribe cualquiera en la red).
2. Si el navegador reconecta el SSE y llega otra vez una `PIEZA` con la misma `epoca`+`seq`, no se cuenta dos veces.
3. En modo supervisión **ninguna** vista contiene un elemento `data-accion` ni la pestaña Etiquetas, y `app.js` ignora acciones aunque se fuerce el clic.
4. Sin ningún dato (modelo vacío, sin piezas, ciclo `null`) no aparece `NaN`, `undefined` ni `null`: aparecen estados vacíos que invitan a la acción.
5. Contenedor con capacidad 0 o con contenido mayor que la capacidad: porcentaje sin dividir entre cero y limitado a 0–100 %.

## Estructura de archivos

```
web2/
├── index.html                  # armazón, <head> con tema, logo y carga de scripts en orden
├── README.md                   # cómo abrirlo y probarlo (Tarea 11)
├── img/                        # favicon.svg, icono-180.png (copiados de docs/marca), simbolo.svg
├── css/
│   ├── tokens.css              # variables de color y tipografía (oscuro en :root, claro en [data-tema="claro"])
│   ├── base.css                # reinicio, tipografía, foco, botones, chips, tablas
│   ├── layout.css              # encabezado, pestañas, rejilla, @media (min-width: 900px), barra inferior
│   └── banda.css               # la banda animada y los contenedores
├── js/
│   ├── formato.js              # esc, hora, duracion, uidCorto, ESTADOS, textoEstado, etiquetaMotivo
│   ├── modo.js                 # deAncho(px), puede(modo, accion)
│   ├── modelo.js               # almacén de datos + selectores
│   ├── fuente-real.js          # cliente de la API de la Pi (SSE + comandos)
│   ├── fuente-demo.js          # generador demo con las mismas salidas
│   ├── etapas.js               # estado del ESP32 → etapa de la banda
│   ├── graficas.js             # sparkline y barras en SVG
│   ├── vistas.js               # funciones puras: encabezado, tiempoReal, inventario, historial, alertas, etiquetas, banda, logo
│   └── app.js                  # único archivo con DOM: elige fuente, pestañas, acciones, tema, anuncios
└── tests/                      # *.test.js con node:test (sin dependencias)
```

Orden de scripts en `index.html`: formato, modo, modelo, fuente-real, fuente-demo, etapas, graficas, vistas, app. Cada JS usa este envoltorio para cargar igual en el navegador y en Node:

```js
(function (root) {
  const Triar = (root.Triar = root.Triar || {});
  Triar.Formato = { /* … */ };
  if (typeof module !== "undefined") module.exports = Triar.Formato;
})(typeof window !== "undefined" ? window : globalThis);
```

Las pruebas hacen `require` de los archivos de los que dependen (en Node, `globalThis.Triar` es compartido). Se ejecutan con `node --test web2/tests/` desde la raíz del repositorio.

---

### Tarea 1: Utilidades de formato

**Archivos:** Crear `web2/js/formato.js`, `web2/tests/formato.test.js`.

**Interfaces — Produce `Triar.Formato`:**
- `esc(valor: any): string` — escapa `& < > " '`; `null`/`undefined` → `""`.
- `hora(ts: number): string` → `"HH:MM:SS"` en hora local. `duracion(ms: number|null): string` → `"4.6 s"` o `"—"`.
- `uidCorto(uid: string|null): string` → `"04A3…00"` (4 primeros + `…` + 2 últimos) o `"Sin etiqueta"`.
- `ESTADOS: Record<string, {texto: string, tono: "ok"|"aviso"|"critico"|"neutro"|"lectura"}>` para los 10 estados del ESP32 (`PAUSA`, `ESPERA`, `LECTURA_NFC`, `CLASIFICANDO`, `LIBERA_INSPECCION`, `CONFIRMA_SALIDA`, `RETORNO`, `PAUSA_CONTENEDOR`, `ERROR`, `EMERGENCIA`). Textos: "En pausa", "Esperando pieza", "Leyendo etiqueta", "Clasificando", "Liberando inspección", "Confirmando salida", "Compuerta al centro", "Contenedor lleno", "Error", "Emergencia". Tonos: `ESPERA`/`CLASIFICANDO`/`LIBERA_INSPECCION`/`CONFIRMA_SALIDA`/`RETORNO` = `ok`; `LECTURA_NFC` = `lectura`; `PAUSA`/`PAUSA_CONTENEDOR` = `aviso`; `ERROR`/`EMERGENCIA` = `critico`.
- `textoEstado(estado: string|null): string` — texto de `ESTADOS`, o `"Sin datos"` si es `null`, o el valor tal cual si es desconocido.
- `etiquetaMotivo(motivo: string|null): string` — `SIN_LECTURA` → "Sin lectura de etiqueta"; `NO_REGISTRADA` → "Etiqueta no registrada"; otro → `""`.

- [ ] **Paso 1: escribir las pruebas que fallan** (`formato.test.js`), con estas aserciones exactas:
  - `esc('<img src=x onerror="a()">&\'')` → `&lt;img src=x onerror=&quot;a()&quot;&gt;&amp;&#39;`; `esc(null)` y `esc(undefined)` → `""`; `esc(5)` → `"5"`.
  - `duracion(4630)` → `"4.6 s"`; `duracion(null)` → `"—"`; `uidCorto("04A31FB26C8000")` → `"04A3…00"`; `uidCorto(null)` → `"Sin etiqueta"`.
  - `hora(new Date(2026,10,24,9,5,7).getTime())` → `"09:05:07"`.
  - Los 10 estados existen en `ESTADOS` con `texto` no vacío y tono permitido; `textoEstado(null)` → `"Sin datos"`; `textoEstado("RARO")` → `"RARO"`.
- [ ] **Paso 2:** `node --test web2/tests/formato.test.js` → FALLA (módulo no existe).
- [ ] **Paso 3:** implementar `web2/js/formato.js` con el envoltorio y las firmas de arriba.
- [ ] **Paso 4:** mismo comando → PASA.
- [ ] **Archivos para `git add`:** `web2/js/formato.js web2/tests/formato.test.js`.

---

### Tarea 2: Modo de pantalla

**Archivos:** Crear `web2/js/modo.js`, `web2/tests/modo.test.js`.

**Interfaces — Produce `Triar.Modo`:**
- `UMBRAL_PX = 900`. `deAncho(px: number): "operacion"|"supervision"` — `px >= 900` → `operacion`.
- `ACCIONES: string[]` = `["paro","vaciado","capacidad","sincronizar","registrar-etiqueta","eliminar-etiqueta","reconocer-alertas","exportar-csv"]`.
- `puede(modo: string, accion: string): boolean` — en `supervision` siempre `false`; en `operacion` solo para acciones de `ACCIONES`.

- [ ] **Paso 1:** pruebas: `deAncho(899)` → `supervision`; `deAncho(900)` → `operacion`; `deAncho(390)` → `supervision`; `puede("supervision", a)` es `false` para **cada** `a` de `ACCIONES`; `puede("operacion","paro")` → `true`; `puede("operacion","borrar-todo")` → `false`.
- [ ] **Paso 2:** ejecutar → FALLA. **Paso 3:** implementar. **Paso 4:** ejecutar → PASA.
- [ ] **Archivos:** `web2/js/modo.js web2/tests/modo.test.js`.

---

### Tarea 3: Modelo de datos

**Archivos:** Crear `web2/js/modelo.js`, `web2/tests/modelo.test.js`.

**Interfaces — Consume:** `Triar.Formato.textoEstado`, `etiquetaMotivo`, `uidCorto`. **Produce `Triar.Modelo.crear(): Modelo`.**

Formas de datos (las del servidor, `rpi/banda/supervisor.py`):
`Pieza {epoca, seq, ts, uid|null, categoria: "A"|"B"|"R", motivo: null|"SIN_LECTURA"|"NO_REGISTRADA", ciclo_ms, reenvio, tardio, nombre|null}` · `Alerta {id?, ts, nivel: "critico"|"advertencia"|"info", codigo, mensaje, reconocida?: boolean}` · `Etiqueta {uid, nombre, categoria}` · `Contenedores {A:{contenido,capacidad}, B:…, R:…}` · instantánea `{enlace_ok, estado, version_tabla, version_esp32, pendientes, etiquetas, piezas (antiguas→recientes), alertas (recientes primero), contenedores, stats}` · eventos `{tipo: "PIEZA"|"ESTADO"|"LATIDO"|"ALERTA"|"CONTENEDORES"|"ETIQUETAS"|"ENLACE", ts, …}`; se añade el evento local `{tipo:"SERVIDOR", ok:boolean}` (conexión del navegador con la Pi).

Campos del modelo: `modo: "real"|"demo"`, `servidorOk`, `enlaceOk`, `estado: string|null`, `piezas`, `alertas`, `etiquetas`, `contenedores`, `versionTabla`, `versionEsp32`, `pendientes`, `ultimoLatidoTs`, `feed: {ts, tipo, texto, tono}[]` (recientes primero, máx. 50, **sin** `LATIDO`).
Métodos: `aplicarInstantanea(inst)`, `aplicar(ev)`, `suscribir(fn): () => void` (se avisa tras cada cambio), y selectores `conteos(): {total,A,B,R,SIN_LECTURA,NO_REGISTRADA}`, `ultimaPieza(): Pieza|null`, `piezasPorMinuto(ahoraMs, minutos = 10): number[]` (un cubo por minuto, del más antiguo al más reciente), `ciclosRecientes(n = 30): number[]` (ms > 0), `uidsSinRegistrar(): string[]` (UID únicos de piezas `NO_REGISTRADA` que no están en `etiquetas`), `alertasSinReconocer(): number`, `porcentajeContenedor(cat): number` (0–100, entero).

- [ ] **Paso 1: pruebas que fallan**, nombre → aserción:
  - `instantánea carga todo`: tras `aplicarInstantanea` con 3 piezas, `conteos().total === 3`, `estado` y `contenedores` copiados.
  - `PIEZA nueva se agrega y avisa una sola vez` (contador de llamadas del suscriptor = 1) y `ultimaPieza().seq` es la nueva.
  - `PIEZA repetida (misma epoca y seq) no duplica` → `piezas.length` no cambia y `conteos().total` tampoco (**punto de revisión 2**).
  - `LATIDO pone enlaceOk true y guarda pendientes`; `ENLACE ok:false` → `enlaceOk false`; `SERVIDOR ok:false` → `servidorOk false`.
  - `ALERTA entra primero; alertasSinReconocer cuenta solo las no reconocidas`.
  - `CONTENEDORES y ETIQUETAS reemplazan los datos`.
  - `feed: máximo 50, nunca contiene LATIDO` (aplicar 60 estados + 5 latidos).
  - `piezasPorMinuto`: 2 piezas en el último minuto y 1 hace 3 min → longitud 10, último cubo 2, `[6]` es 1, el resto 0.
  - `ciclosRecientes ignora null y 0`. `uidsSinRegistrar` excluye UID ya presentes en `etiquetas` y no repite.
  - **Modelo vacío** (**punto 4**): `conteos()` todo en 0, `ultimaPieza()` `null`, `piezasPorMinuto(Date.now())` diez ceros, `ciclosRecientes()` `[]`, sin lanzar errores.
  - `porcentajeContenedor` (**punto 5**): `{contenido:7,capacidad:15}` → 47; `{contenido:3,capacidad:0}` → 0; `{contenido:20,capacidad:15}` → 100.
- [ ] **Paso 2:** `node --test web2/tests/modelo.test.js` → FALLA.
- [ ] **Paso 3:** implementar. Piezas ordenadas por `(epoca, seq)`; deduplicar con un `Set` de `"epoca:seq"`; el texto del feed usa `etiquetaMotivo`/`uidCorto` (ej. "Pieza #42 → A", "Pieza #43 rechazada: Etiqueta no registrada", "Estado: Leyendo etiqueta"; tono `critico` para alertas críticas y emergencia).
- [ ] **Paso 4:** ejecutar → PASA.
- [ ] **Archivos:** `web2/js/modelo.js web2/tests/modelo.test.js`.

---

### Tarea 4: Fuente real (cliente de la API de la Pi)

**Archivos:** Crear `web2/js/fuente-real.js`, `web2/tests/fuente-real.test.js`.

**Interfaces — Consume:** `Modelo` (T3). **Produce `Triar.FuenteReal`:**
- `conectar(modelo, deps: {fetch, EventSource, base?: string}): Promise<{cerrar(): void}>` — hace `GET {base}api/estado`; si falla o responde no-OK, **rechaza** (la app pasa a demo). Si funciona: `modelo.modo = "real"`, `aplicarInstantanea`, abre `EventSource({base}api/eventos)`; cada mensaje se `JSON.parse` y va a `modelo.aplicar`; `onerror` → `aplicar({tipo:"SERVIDOR", ok:false})`; `onopen` → `ok:true`.
- `comandos(deps): { paro(), vaciado(cat), capacidad(cat, n), sincronizar(), registrarEtiqueta(uid, nombre, cat), eliminarEtiqueta(uid), reconocerAlertas() }` — cada uno devuelve `Promise<{ok: true, mensaje?: string, version?: number}>` y **lanza `Error` con el texto del servidor** (`error` o `mensaje` del JSON) si la respuesta es 4xx/5xx. Rutas: `POST api/comando` con `{cmd:"PARO"}`, `{cmd:"VACIADO",categoria}`, `{cmd:"CAPACIDAD",categoria,capacidad}`, `{cmd:"SYNC"}`; `POST api/etiquetas` con `{uid,nombre,categoria}`; `DELETE api/etiquetas/<uid codificado>`; `POST api/alertas/reconocer`. Cuerpos JSON con `Content-Type: application/json`.

- [ ] **Paso 1: pruebas** con `fetch` y `EventSource` falsos (clase que guarda `onmessage/onerror/onopen`):
  - `conectar carga la instantánea y marca modo real`; `conectar rechaza si /api/estado falla` y también si responde 500.
  - `un mensaje SSE llega al modelo` (PIEZA → `conteos().total` sube); `onerror` deja `servidorOk` en `false` y `onopen` lo devuelve a `true`.
  - `paro envía POST api/comando con {"cmd":"PARO"}` (verificar método, URL y cuerpo).
  - `vaciado("A")`, `capacidad("B", 20)`, `registrarEtiqueta`, `eliminarEtiqueta("04:A3")` (URL con `encodeURIComponent`).
  - `error 400 con {"ok":false,"error":"UID inválido"} lanza Error("UID inválido")`; con `{"ok":false,"mensaje":"x"}` lanza `Error("x")`.
- [ ] **Paso 2:** ejecutar → FALLA. **Paso 3:** implementar. **Paso 4:** ejecutar → PASA.
- [ ] **Archivos:** `web2/js/fuente-real.js web2/tests/fuente-real.test.js`.

---

### Tarea 5: Modo demo propio

**Archivos:** Crear `web2/js/fuente-demo.js`, `web2/tests/fuente-demo.test.js`.

**Interfaces — Consume:** `Modelo` (T3). **Produce `Triar.FuenteDemo`:**
- `crear(modelo, deps?: {azar?: () => number, ahora?: () => number, setInterval?, clearInterval?, velocidad?: number}): Demo` — `velocidad` (por defecto 1) divide las duraciones de cada estado; `app.js` la toma de `?velocidad=N` en la URL (útil para capturas y ensayos).
- `Demo.iniciar(): void` (pone `modelo.modo = "demo"`, carga una instantánea inicial con 3 etiquetas inventadas —una A, una B, una R—, contenedores `{A:{contenido:0,capacidad:15},B:…,R:…}`, y programa `avanzar()` con los tiempos de abajo), `Demo.detener()`, `Demo.avanzar(): void` (da **un** paso de la máquina y emite sus eventos; es lo que prueban los tests).
- `Demo.inyectar(falla: "SIN_ETIQUETA"|"NO_REGISTRADA"|"ATASCO"|"ENLACE_CAIDO"|"CONTENEDOR_LLENO"): void`.
- `Demo.comandos` — **misma superficie que `FuenteReal.comandos`** (mismos nombres y promesas), actuando sobre el estado local; más `Demo.restablecer()` (solo demo; sustituye al botón físico).

Máquina del demo (mismos estados y mensajes que el servidor): `ESPERA → LECTURA_NFC → CLASIFICANDO → LIBERA_INSPECCION → CONFIRMA_SALIDA → (evento PIEZA) → RETORNO → ESPERA`. Un `ESTADO` por paso; `LATIDO` en cada paso; `PIEZA` con `seq` creciente, `epoca: 1`, `ciclo_ms` entre 4000 y 5500, categoría según la tabla local (UID inventado conocido → A/B/R; `null` → `R`+`SIN_LECTURA`; UID no registrado → `R`+`NO_REGISTRADA`), y contenedor +1. Duración de cada estado: 1500/1200/600/800/600/800 ms (inventados).
Fallas: `ATASCO` → `ESTADO ERROR` + `ALERTA critico ATASCO` y no avanza hasta `restablecer()`; `ENLACE_CAIDO` → `ENLACE ok:false` y 8 s después `ok:true`; `CONTENEDOR_LLENO` → el contenedor del próximo destino queda en capacidad y se emite `PAUSA_CONTENEDOR` + alerta `advertencia`, hasta `comandos.vaciado(cat)`; `comandos.paro()` → `EMERGENCIA` + alerta crítica; `registrarEtiqueta` agrega a la tabla local y emite `ETIQUETAS`.

- [ ] **Paso 1: pruebas** (con `azar` determinista y sin temporizadores reales, llamando `avanzar()`):
  - `una pieza normal recorre los estados en orden y emite una PIEZA con seq 1` (leer `modelo.feed`/`piezas`).
  - `la categoría sigue la tabla local` (con `azar` forzado a un UID conocido → su categoría; contenedor sube 1).
  - `inyectar SIN_ETIQUETA → R con motivo SIN_LECTURA y uid null`; `NO_REGISTRADA → R, motivo NO_REGISTRADA y el UID aparece en uidsSinRegistrar()`.
  - `registrarEtiqueta de ese UID → la siguiente pieza con ese UID va a su categoría` y `uidsSinRegistrar()` ya no lo incluye.
  - `ATASCO → estado ERROR, alerta crítica; avanzar() no cambia nada; restablecer() vuelve a ESPERA`.
  - `CONTENEDOR_LLENO → PAUSA_CONTENEDOR hasta comandos.vaciado`; `paro → EMERGENCIA`.
  - `ENLACE_CAIDO → enlaceOk false` (los 8 s se prueban con el `ahora` inyectado).
  - `Demo.comandos tiene los mismos métodos que FuenteReal.comandos(...)` (comparar `Object.keys`).
- [ ] **Paso 2:** ejecutar → FALLA. **Paso 3:** implementar (sin reutilizar `web/js/simulador.js`). **Paso 4:** ejecutar → PASA.
- [ ] **Archivos:** `web2/js/fuente-demo.js web2/tests/fuente-demo.test.js`.

---

### Tarea 6: Etapas de la banda y gráficas

**Archivos:** Crear `web2/js/etapas.js`, `web2/js/graficas.js`, `web2/tests/etapas.test.js`, `web2/tests/graficas.test.js`.

**Interfaces — Produce:**
- `Triar.Etapas.ORDEN = ["entrada","inspeccion","lector","salida","compuerta"]`.
- `Triar.Etapas.de(estado: string|null): {etapa: string|null, tono: "ok"|"aviso"|"critico"|"neutro"|"lectura"}` — `ESPERA`→`entrada`; `LIBERA_INSPECCION`→`inspeccion`; `LECTURA_NFC`→`lector` (tono `lectura`); `CONFIRMA_SALIDA`→`salida`; `CLASIFICANDO` y `RETORNO`→`compuerta`; `PAUSA`, `PAUSA_CONTENEDOR`, `ERROR`, `EMERGENCIA`, `null` → `etapa: null` (tono de `ESTADOS`; `neutro` si `null`).
- `Triar.Graficas.sparkline(valores: number[], o: {ancho: number, alto: number}): string` → HTML de un `<svg role="img" aria-label=…>` con un `<polyline>`; con lista vacía o toda en cero devuelve una línea plana (nunca `NaN`).
- `Triar.Graficas.barras(valores: number[], o: {ancho, alto}): string` → `<svg>` con una `<rect>` por valor, altura relativa al máximo (altura mínima 1 px para valores > 0).

- [ ] **Paso 1: pruebas:** las 10 transiciones de `Etapas.de`; `sparkline([])` y `sparkline([0,0,0])` no contienen `NaN`; `sparkline([1,2,3],{ancho:100,alto:20})` contiene `<polyline` y puntos dentro de `[0,100]×[0,20]`; `barras([0,5,10])` devuelve 3 `<rect`.
- [ ] **Pasos 2–4:** ejecutar → FALLA; implementar; ejecutar → PASA.
- [ ] **Archivos:** los cuatro anteriores.

---

### Tarea 7: Estilos, armazón y arranque de la app

**Archivos:** Crear `web2/index.html`, `web2/css/tokens.css`, `web2/css/base.css`, `web2/css/layout.css`, `web2/css/banda.css`, `web2/js/vistas.js` (solo `logo`, `encabezado`, `pestanas` en esta tarea), `web2/js/app.js`, `web2/img/` (copiar `docs/marca/export-simbolo/simbolo-favicon.svg` → `favicon.svg`, `icono-180.png`, y `docs/marca/simbolo.svg`), `web2/tests/contraste.test.js`, `web2/tests/vistas-base.test.js`.

**Interfaces:**
- `tokens.css`: oscuro en `:root`; claro en `:root[data-tema="claro"]`; variables `--fondo --superficie --texto --texto-2 --texto-3 --acento --sobre-acento --cat-a --cat-b --cat-r --ok --aviso --critico` con **exactamente** los valores de las restricciones globales; `@media (prefers-contrast: more)` refuerza bordes y `--texto-2`.
- `vistas.js` — `Triar.Vistas.logo(variante: "simbolo"|"horizontal"): string` (SVG inline con `fill/stroke: currentColor`, `role="img"`, `aria-label="Triar"`, trazos de `docs/marca/`); `encabezado(modelo, {modo, ahora}): string` (logo + "Banda Clasificadora", chip de **enlace** con texto "Enlace con el ESP32: conectado/sin latido", chip de **estado** con `textoEstado`, banner "Sin conexión con la Pi; reintentando" si `!servidorOk`, y `PARO` solo si `modo === "operacion"`); `pestanas(modo): string` (5 pestañas en operación, 4 en supervisión; botones con `data-vista`).
- `app.js` — en `<head>`, un script en línea fija `document.documentElement.dataset.tema` (`?tema=` o `matchMedia("(prefers-color-scheme: light)")`, y escucha cambios). `app.js` hace: `Modo.deAncho(innerWidth)` con `matchMedia("(min-width: 900px)")` → `document.body.dataset.modo`; elige fuente (`FuenteReal.conectar`; si rechaza o la URL trae `?demo` o es `file://` → `FuenteDemo`, que muestra la franja de demo); pinta con `innerHTML` al recibir cambios del modelo (agrupados con `requestAnimationFrame`); delegación de eventos para `[data-vista]` (pestañas, con `aria-selected`) y `[data-accion]` (**comprueba `Modo.puede(modo, accion)` antes de ejecutar**); región `aria-live="polite"` que anuncia solo cambios de **estado** (no cada pieza); pie con "Panel en `http://<location.host>`".

- [ ] **Paso 1: pruebas que fallan:**
  - `contraste.test.js`: lee `tokens.css`, extrae los pares y calcula contraste WCAG; exige ≥ 4.5 para `--texto`, `--texto-2`, `--texto-3`, `--cat-a`, `--cat-b`, `--cat-r`, `--ok`, `--aviso`, `--critico`, `--acento` sobre `--superficie` y sobre `--fondo` (solo texto/acento sobre ambos; el resto sobre superficie), y `--sobre-acento` sobre `--acento`, **en el bloque oscuro y en el claro**.
  - `vistas-base.test.js`: `pestanas("supervision")` tiene 4 `data-vista` y no contiene `etiquetas`; `pestanas("operacion")` tiene 5; `encabezado` con `modo:"supervision"` **no** contiene `data-accion`; con `"operacion"` contiene `data-accion="paro"` y la leyenda "Paro por software; el seguro es el botón físico"; muestra "Sin conexión con la Pi" cuando `servidorOk` es `false`; con un `estado` desconocido o `null` no contiene `undefined`/`null`.
- [ ] **Paso 2:** `node --test web2/tests/` → FALLA en estas dos.
- [ ] **Paso 3:** implementar CSS, `vistas.js` parcial, `index.html` y `app.js`.
- [ ] **Paso 4:** `node --test web2/tests/` → todo PASA.
- [ ] **Paso 5: verificación visual.** Con Firefox headless (usar perfil temporal y `timeout`):
  ```bash
  P=$(mktemp -d); timeout 60 firefox --headless --no-remote --profile "$P" \
    --screenshot /tmp/triar-ancho.png --window-size=1280,800 "file://$PWD/web2/index.html?demo"
  ```
  y otra a `--window-size=390,844`. Abrir ambas PNG: se ve el encabezado, las pestañas (arriba en ancha, abajo en teléfono), la franja de demo y el tema oscuro; en la de 390 px no aparece `PARO`.
- [ ] **Archivos:** todos los de esta tarea.

---

### Tarea 8: Vista Tiempo real (banda, cifras, gráficas, feed)

**Archivos:** Modificar `web2/js/vistas.js` (añadir `banda`, `tiempoReal`), `web2/css/banda.css`, `web2/js/app.js` (montar la vista); Crear `web2/tests/vistas-tiempo-real.test.js`.

**Interfaces — Consume:** `Modelo`, `Etapas.de`, `Graficas`, `Formato`. **Produce:**
- `Vistas.banda(modelo, {orientacion: "horizontal"|"vertical", ahora: number}): string` — un solo SVG/HTML: nodos de `Etapas.ORDEN` con su texto ("Entrada", "Sensor de inspección", "Lector NFC", "Sensor de salida", "Compuerta"), la etapa activa con clase `activa` y atributo `aria-current="step"`, tono en `data-tono`, y los tres contenedores A/B/R con ocupación `contenido/capacidad` y barra; el contenedor de la última pieza lleva clase `destino` solo si `ahora - ts < 2500`. Con `etapa: null` (pausa/emergencia/error) ninguna etapa activa y el tono del estado visible junto a un texto.
- `Vistas.tiempoReal(modelo, {modo, ahora}): string` — banda (horizontal si `modo === "operacion"`, vertical en supervisión; el CSS puede ajustar), tarjeta "Última pieza" ("Llave → Contenedor A", UID corto en monoespaciada, ciclo), cifras (total, A, B, Rechazo, ciclo promedio), gráficas (piezas por minuto con `barras`, tiempo de ciclo con `sparkline`; solo en operación) y feed (en teléfono solo los 5 más recientes).
- Estado vacío (**punto 4**): sin piezas muestra "Aún no hay piezas. Coloca una pieza con etiqueta en la banda."; en demo añade "o espera a la demostración".

- [ ] **Paso 1: pruebas:**
  - `la etapa activa sigue al estado` (LECTURA_NFC → `lector` con `aria-current="step"`; ESPERA → `entrada`).
  - `EMERGENCIA y PAUSA no marcan ninguna etapa` y muestran el texto del estado.
  - `el contenedor destino se resalta solo 2.5 s` (con `ahora` dentro y fuera de la ventana).
  - **Punto 1:** una pieza con `nombre:'<img src=x onerror=alert(1)>'` aparece como `&lt;img` y la salida no contiene `<img`.
  - **Punto 3:** `tiempoReal(..., {modo:"supervision"})` no contiene `data-accion`.
  - **Punto 4:** modelo vacío → contiene el texto de estado vacío y ni `NaN`, `undefined` ni `null`.
  - **Punto 5:** capacidad 0 y contenido > capacidad → porcentajes 0 y 100 en la barra (`style="--p:0%"`/`"--p:100%"` o equivalente), sin `NaN`.
  - Supervisión: el feed tiene como máximo 5 elementos.
- [ ] **Paso 2:** ejecutar → FALLA. **Paso 3:** implementar vista y `banda.css` (**firma**: etapas que se iluminan; el lector "respira" en cian durante `LECTURA_NFC`; con `@media (prefers-reduced-motion: reduce)` la animación se reemplaza por cambio de color). **Paso 4:** ejecutar → PASA.
- [ ] **Paso 5: verificación visual:** repetir las dos capturas de la Tarea 7 con la URL `?demo&velocidad=8` (la captura se toma al cargar, así que con velocidad alta ya hay piezas y contenedores con datos; si aún se ve vacío, añadir `--screenshot-delay` no existe en Firefox: en su lugar abrir la página en el navegador y capturar a mano) y revisar que la banda, el lector y los contenedores se ven y no hay desbordes en 390 px.
- [ ] **Archivos:** los de la tarea.

---

### Tarea 9: Inventario, Historial y Alertas

**Archivos:** Modificar `web2/js/vistas.js` (añadir `inventario`, `historial`, `alertas`), `web2/css/base.css`, `web2/js/app.js`; Crear `web2/tests/vistas-listas.test.js`.

**Interfaces — Produce** (todas con `{modo, ahora}`):
- `Vistas.inventario(modelo, o)` — tarjeta por contenedor (nombre, `contenido de capacidad`, barra con `porcentajeContenedor`); tabla por UID (nombre, categoría, cantidad, última lectura). En operación añade, por contenedor, botones `data-accion="vaciado" data-categoria="A"` ("Marcar vaciado") y un campo + botón `data-accion="capacidad"` ("Cambiar capacidad", valor 1–999); en supervisión **no** hay controles.
- `Vistas.historial(modelo, o)` — tabla (hora, número, objeto, destino, motivo, ciclo, "En vivo"/"Reenviada"); máximo 200 filas en operación y 20 en supervisión. En operación: filtro por categoría (`data-filtro`) y botón `data-accion="exportar-csv"` ("Exportar CSV", enlace a `api/historial.csv`, oculto en demo).
- `Vistas.alertas(modelo, o)` — lista con nivel (icono + texto "Crítica"/"Advertencia"/"Info", nunca solo color), código, mensaje, hora; las no reconocidas destacadas. En operación: botón `data-accion="reconocer-alertas"` ("Reconocer todas") deshabilitado si no hay pendientes.
- Vacíos: "Sin piezas todavía", "Sin alertas. Todo en orden." (sin `undefined`).

- [ ] **Paso 1: pruebas:** supervisión → `inventario`, `historial`, `alertas` **sin** `data-accion`; operación contiene los esperados; **punto 1** (nombre con HTML escapado en inventario e historial; mensaje de alerta con `<script>` escapado); **punto 4** (vacíos); **punto 5** (barra del contenedor sin `NaN`); `historial` en supervisión no pasa de 20 filas con 50 piezas; el nivel de alerta aparece como texto.
- [ ] **Pasos 2–4:** ejecutar → FALLA; implementar; ejecutar → PASA.
- [ ] **Archivos:** los de la tarea.

---

### Tarea 10: Etiquetas (solo pantalla ancha) y acciones

**Archivos:** Modificar `web2/js/vistas.js` (añadir `etiquetas`), `web2/js/app.js` (acciones, avisos); Crear `web2/tests/vistas-etiquetas.test.js`.

**Interfaces — Produce:** `Vistas.etiquetas(modelo, {modo}): string` — si `modo !== "operacion"` devuelve `""`. En operación: formulario (UID, nombre, categoría A/B/R) con `data-accion="registrar-etiqueta"`; lista "Etiquetas detectadas sin registrar" (de `uidsSinRegistrar()`) con botón de registro en un clic (rellena el UID); tabla de etiquetas con `data-accion="eliminar-etiqueta" data-uid=…`; botón "Sincronizar con ESP32" (`data-accion="sincronizar"`) y un aviso "La tabla del ESP32 está desactualizada (v{versionEsp32} de v{versionTabla})" cuando difieren.
`app.js`: las acciones llaman a `comandos` de la fuente activa; toda promesa rechazada muestra el mensaje en un aviso (`role="status"`, desaparece a los 6 s); eliminar etiqueta pide `confirm("¿Eliminar la etiqueta {nombre}?")` (cancelar no hace nada); `paro` **no** pide confirmación; en demo `registrar-etiqueta` normaliza el UID (hex, con o sin `:`) y muestra el error "UID inválido: usa hexadecimal, con o sin ':'." si no lo es.

- [ ] **Paso 1: pruebas:** `etiquetas(..., {modo:"supervision"})` → `""` (**punto 3**); operación incluye formulario y botones; el aviso de versión solo aparece cuando `versionEsp32 !== versionTabla`; `uidsSinRegistrar` produce botones de registro rápido; nombre con HTML escapado en la tabla (**punto 1**); lista vacía → "Aún no hay etiquetas. Registra la primera arriba."
- [ ] **Paso 2:** ejecutar → FALLA. **Paso 3:** implementar; en `app.js` verificar a mano que forzar `data-accion` en modo supervisión (desde la consola del navegador) no ejecuta nada. **Paso 4:** `node --test web2/tests/` → PASA.
- [ ] **Archivos:** los de la tarea.

---

### Tarea 11: Accesibilidad, integración con la Pi y documentación

**Archivos:** Crear `web2/README.md`; Modificar `README.md` (raíz, línea de `web/`), `docs/superpowers/specs/2026-11-15-panel-nuevo-design.md` (§6.3: orden de la banda con sensor de salida; nota del tema por URL).

- [ ] **Paso 1: recorrido de accesibilidad** (a mano, con las capturas y el navegador): todo se maneja con teclado y el foco se ve; botones de icono tienen texto o `aria-label`; con el sistema en "reducir movimiento" la banda no se anima (en Firefox: `about:config` → `ui.prefersReducedMotion` = 1); texto al 200 % no desborda en 390 px; contrastes ya cubiertos por la prueba de la Tarea 7.
- [ ] **Paso 2: integración con la Pi simulada.** En tres terminales (desde `rpi/`): `python3 emulador_esp32.py` (anota el puerto `/dev/pts/N`), `python3 servidor.py --puerto /dev/pts/N --web ../web2`, y abrir `http://localhost:8080`. Comprobar: el encabezado dice conectado y **sin** franja de demo; llegan piezas; registrar una etiqueta desde la pestaña Etiquetas funciona y el aviso de versión aparece y se apaga tras sincronizar; `Exportar CSV` descarga; en el emulador la tecla `c` (enlace caído) muestra "sin latido"; cortar el servidor muestra "Sin conexión con la Pi; reintentando" y al volver desaparece; **abrir la misma dirección en un teléfono real conectado a la red de la Pi**: solo hay supervisión, 4 pestañas, sin `PARO`.
- [ ] **Paso 3:** `cd rpi && python3 -m unittest discover -s tests -t . -v` → siguen pasando las 37 pruebas (no se tocó el servidor); `node --test web2/tests/` → todo PASA.
- [ ] **Paso 4: documentación** (usar la skill `repo-docs`): `web2/README.md` en español con cómo abrirlo (real y demo), la tabla de secciones por dispositivo, estructura de archivos, cómo correr las pruebas, y una sección "Qué está probado y qué no" (probado: pruebas automáticas y emulador; **no probado:** Raspberry Pi real, ESP32 real, punto de acceso Wi-Fi de la Pi); actualizar la línea de `web2/` en el README raíz.
- [ ] **Paso 5:** informar al usuario con la lista de archivos nuevos/modificados, qué se verificó y cómo, y **preguntar si desea hacer el commit**.
- [ ] **Archivos:** `web2/README.md README.md docs/superpowers/specs/2026-11-15-panel-nuevo-design.md`.

---

## Revisión del plan frente al spec

| Sección del spec | Tarea |
|---|---|
| §2 antimetas (sin internet, teléfono sin cambios, colores de categoría ≠ estado) | globales; T3 (modelo), T7 (tokens, contraste), T8–T10 (sin `data-accion` en supervisión) |
| §4 decisiones (paro, animación por estado, gráficas, tema, 900 px, banda vertical, nombre) | T2, T6, T7, T8, T10 |
| §6.1 color · §6.2 tipografía · §6.3 firma | T7 (tokens, escala), T8 (banda y reducción de movimiento) |
| §7 distribución · §8 secciones | T7 (armazón y pestañas), T8–T10 |
| §9 datos y API | T3, T4 |
| §10 modo demo propio | T5 |
| §11 accesibilidad e interacción | T7 (`aria-live`, foco), T10 (confirmación y errores), T11 |
| §12 pruebas y verificación | T1–T10 (automáticas), T7/T8/T11 (visuales e integración) |
| §14 red de la Pi | T7 (muestra `location.host`), T11 (prueba con teléfono) |

Consistencia de nombres revisada: `Modelo.crear`, `aplicarInstantanea`, `aplicar`, `FuenteReal.conectar/comandos`, `FuenteDemo.crear` (con `Demo.comandos` idéntico a `FuenteReal.comandos`), `Etapas.de`, `Modo.puede`, `Vistas.*` con `{modo, ahora}`.
