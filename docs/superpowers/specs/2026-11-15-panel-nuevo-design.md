# Panel nuevo (`web2/`) — diseño

> **Estado:** borrador para aprobación. No hay código todavía.
> Fecha: 2026-11-15. Entrega del proyecto: martes 24 de noviembre.

## 1. Intención

Una interfaz nueva para supervisar y operar la banda clasificadora, hecha desde cero. El panel actual (`web/`) se
considera una versión de prueba y simulación: **no se reutiliza su código ni su aspecto**; se queda intacto como respaldo.

Se mostrará el día de la entrega en **una laptop** (operación y supervisión) y en **un teléfono** (solo supervisión).
Tiene que verse bien, entenderse sin explicación y funcionar sin internet (la Raspberry Pi puede no tener red).

## 2. Metas, no metas y antimetas

**Metas**
- Que quien mire la pantalla entienda en un segundo qué hace la banda: en qué estado está, a dónde fue la última pieza y cuánto hay en cada contenedor.
- Que la laptop permita todo lo que permite hoy el panel (etiquetas, vaciado, capacidad, paro, alertas, historial, CSV).
- Que el teléfono muestre el estado y los resultados con letra legible y sin formularios.
- Seguir las guías de Apple (HIG) en accesibilidad, color, tipografía y navegación, con identidad propia en el color y en la banda.

**No metas**
- No hay login ni PIN (mejora futura; hoy tampoco existe, ver `rpi/README.md` §4).
- No se cambia el servidor de la Pi ni la API ni el firmware.
- No se muestra la posición física real de la pieza (el sistema no la mide).
- No hay interruptor claro/oscuro propio.

**Antimetas (sería un fracaso aunque funcione)**
- Que parezca una plantilla genérica de dashboard.
- Que en el teléfono se pueda modificar algo.
- Que dependa de internet, de una librería externa o de un paso de compilación.
- Que un color de categoría se confunda con uno de estado (que "contenedor B" parezca una advertencia).

## 3. Decisiones del usuario

| Tema | Decisión |
|---|---|
| Distribución | **Línea de producción**: la banda ocupa el ancho arriba, con sensores, lector, compuerta y contenedores al final; debajo cifras y feed |
| Paleta | **Taller oscuro** (oscuro primero, cian de lectura NFC) |
| Tipografía | **Sistema Apple**: fuente del sistema para todo, monoespaciada del sistema para UID, hora y números |
| Dispositivos | Laptop y teléfono |
| Teléfono | **Solo supervisión** |
| Cómo se decide | **Por tamaño de pantalla**: angosta = supervisión; ancha = supervisión + controles |
| Modo demo | **Propio**, con datos inventados, sin ESP32 |
| Cómo construirlo | HTML, CSS y JS puros en `web2/` (opción 1) |

## 4. Decisiones mías (a revisar)

- **Paro:** solo en pantallas anchas, siempre visible arriba a la derecha, de un toque y sin confirmación. Leyenda: "paro por software; el seguro es el botón físico". Arrancar y restablecer siguen siendo solo del botón físico.
- **Animación de la banda:** sigue el *estado* del ESP32 (cada etapa se ilumina), no una posición real. Es la limitación ya documentada en `web/README.md` §7.
- **Gráficas:** dos pequeñas en Tiempo real: piezas por minuto y tiempo de ciclo.
- **Modo claro:** se sigue el modo del sistema (`prefers-color-scheme`); el oscuro es el de diseño y el claro está derivado.
- **Umbral de pantalla ancha:** 900 px de ancho (aquí se activan controles y barra superior). Es una convención de diseño adaptable, no seguridad: una tablet o un "sitio de escritorio" en el teléfono mostraría los controles.
- **Banda en teléfono:** vertical y angosta, con la pieza bajando hacia los tres contenedores. En laptop, horizontal. Es el mismo componente.
- **Nombre y logotipo (decidido):** el panel se llama **Triar** y usa el logotipo de [`docs/marca/`](../../marca/README.md) (símbolo de anillo con tres salidas). Encabezado: logo horizontal en laptop, solo símbolo en teléfono. Favicon: `simbolo-pequeno` sobre mosaico oscuro. Subtítulo de apoyo en pantalla: "Banda Clasificadora".

## 5. Enfoques considerados

1. **HTML, CSS y JS puros en `web2/` — elegido.** Se sirve con `python3 servidor.py --web ../web2`, sin tocar la Pi. Sin instalación ni compilación. El código queda más repetitivo.
2. **Svelte o Preact.** Más ordenado, pero exige compilar en la PC y copiar el resultado a la Pi; demasiado para 5 pantallas y 9 días.
3. **Reformar `web/` en su lugar.** Descartada: el usuario no quiere usar esa versión y mezclar viejo con nuevo ensucia el resultado.

## 6. Sistema visual

### 6.1 Color

Fondo y superficies oscuros, texto claro. Cian solo para lo que se lee o se actúa *ahora*.

| Rol | Oscuro | Claro (derivado) | Contraste (sobre superficie) |
|---|---|---|---|
| Fondo | `#0E1417` | `#F2F5F6` | — |
| Superficie | `#172127` | `#FFFFFF` | — |
| Texto | `#E8EEF0` | `#0E1417` | 14.0 / 18.6 |
| Texto secundario | `#9AA9B1` | `#4B5A62` | 6.8 / 7.1 |
| Texto terciario | `#8A9AA3` | `#5A6770` | 5.6 / 5.8 |
| Acento (lector, acción principal) | `#38D6C8` (texto del botón `#0E1417`: 10.3) | `#00766E` (texto del botón blanco: 5.5) | 10.3 / 5.5 |
| Categoría A | `#5AA9FF` | `#1F67C9` | 6.7 / 5.5 |
| Categoría B | `#FF7AB6` | `#B8185F` | 6.8 / 6.3 |
| Rechazo R | `#9AA9B1` | `#5A6770` | 6.8 / 5.8 |
| OK | `#4ADE80` | `#167A32` | 9.4 / 5.4 |
| Aviso | `#FBBF24` | `#8F5400` | 9.8 / 6.1 |
| Crítico | `#FF6B5E` | `#C0342A` | 5.9 / 5.6 |

Reglas: un color significa una sola cosa; ningún estado se comunica solo con color (siempre hay texto o forma); con
`prefers-contrast: more` se refuerzan bordes y texto secundario. Todos los contrastes son ≥ 4.5:1 (calculados;
`color.md`, `accessibility.md`).

### 6.2 Tipografía

- Todo en `system-ui, -apple-system, "Segoe UI", Roboto, sans-serif`; datos en `ui-monospace, "SF Mono", Menlo, Consolas, monospace`.
- Cuerpo 17 px (16 px mínimo en laptop); nada menor a 12 px. Escala: 12 / 14 / 17 / 22 / 34 / 48 px; la de 48 es solo para el estado en grande.
- Jerarquía con tamaño y peso (600–700 para títulos); sin pesos finos en texto pequeño. Todo en `rem` para que respete el tamaño de letra del sistema.
- Para la demo el aspecto cambia un poco entre dispositivos (SF en iPhone/Mac, Roboto en Android, Segoe UI en Windows); es un costo aceptado.

### 6.3 Firma

La **banda animada** (único elemento audaz): cada etapa (entrada → sensor de inspección → lector → compuerta → contenedores) se ilumina según el estado del ESP32 y el lector respira en cian durante `LECTURA_NFC`. Todo lo demás es discreto. Con `prefers-reduced-motion` la animación se reemplaza por cambio de color sin movimiento.

## 7. Distribución

**Laptop (≥ 900 px)**

```
┌────────────────────────────────────────────────────────────────┐
│ Banda Clasificadora   Tiempo real  Inventario  Etiquetas  Historial  Alertas │ ● UART · [PARO] │
├────────────────────────────────────────────────────────────────┤
│  entrada ── HC-SR04 ── RC522 ── HC-SR04 ── compuerta ─▶ [A] [B] [R]  │
├───────────────────────┬────────────────────────────────────────┤
│ Última pieza · Total · Ciclo        │ Feed de eventos                    │
│ Piezas/min · Tiempo de ciclo        │                                    │
└───────────────────────┴────────────────────────────────────────┘
```

**Teléfono (< 900 px)**: una columna, solo supervisión.

```
┌──────────────┐
│ ESTADO grande│  ← LECTURA NFC · enlace
│ Última pieza │
│  banda       │  ← vertical, angosta
│  vertical    │
│ A  B  R      │  ← contenedores
├──────────────┤
│ ◉  ▤  ≡  ⚠   │  ← barra inferior
└──────────────┘
```

## 8. Secciones

| Sección | Laptop | Teléfono |
|---|---|---|
| Tiempo real | banda, cifras, última pieza, gráficas, feed | estado, última pieza, banda vertical, contenedores, feed corto |
| Inventario | tarjetas por contenedor (ocupación, capacidad, **Marcar vaciado**, cambiar capacidad) y tabla por UID | solo lectura: ocupación y conteo por objeto |
| Etiquetas | formulario, UID sin registrar, tabla, **Sincronizar**, eliminar | **no aparece** |
| Historial | tabla completa, filtro, **Exportar CSV** | lista de las últimas piezas, sin exportar |
| Alertas | lista, **Reconocer todas** | lista, sin reconocer |

Estado de sistema (modo, enlace UART, estado del ESP32) siempre visible en el encabezado. Navegación: pestañas arriba en
laptop; barra inferior de 4 pestañas en teléfono (Etiquetas no está).

## 9. Datos y API (sin cambios)

Se usa la API que ya sirve la Pi (`rpi/README.md` §4): `GET /api/estado`, `GET /api/eventos` (SSE),
`GET /api/historial.csv`, `POST /api/comando` (`PARO`, `VACIADO`, `CAPACIDAD`, `SYNC`), `POST /api/etiquetas`,
`DELETE /api/etiquetas/<uid>`, `POST /api/alertas/reconocer`. Los mensajes y estados están en `web/README.md` §5.
Estados de la máquina: `PAUSA`, `ESPERA`, `LECTURA_NFC`, `CLASIFICANDO`, `LIBERA_INSPECCION`, `CONFIRMA_SALIDA`,
`RETORNO`, `PAUSA_CONTENEDOR`, `ERROR`, `EMERGENCIA`.

Como en el panel actual, los datos entran por una sola función y los comandos salen por otra, de modo que real y demo
comparten toda la interfaz.

## 10. Modo demo propio

Un generador pequeño dentro de `web2/` (no reutiliza `web/js/simulador.js`) que emite los mismos mensajes que el
servidor: piezas A, B, rechazo, cambios de estado, latidos y alertas, con etiquetas inventadas. Se activa con `?demo` o
si no hay servidor. Una franja visible dice **"Modo demostración — datos inventados"**. Incluye botones para provocar
sin ESP32: pieza sin etiqueta, atasco, enlace caído y contenedor lleno (solo laptop). Ningún valor del demo es un
resultado experimental.

## 11. Accesibilidad e interacción

- Objetivos táctiles y de ratón de 44 px mínimo en teléfono y 28 px en laptop; foco de teclado visible en todo.
- Cada icono lleva texto o `aria-label`; los cambios de estado se anuncian con `aria-live` sin saturar (el feed no se anuncia pieza por pieza).
- Estados: cargando (algo visible al instante), error de conexión con instrucción ("Sin conexión con la Pi; reintentando"), vacíos que invitan a la acción.
- Destructivo (eliminar etiqueta) con confirmación y cancelar; `PARO` sin confirmación a propósito.
- Textos de botones con verbo ("Marcar vaciado", "Sincronizar con ESP32"), en español, título solo en la primera palabra.

## 12. Pruebas y verificación

- Revisar a mano en la laptop y en un teléfono real (o emulación de 390 px) tanto en modo demo como contra `rpi/emulador_esp32.py`.
- Contraste y tamaños con valores reales (la tabla de §6.1); modo claro y oscuro; `prefers-reduced-motion`; texto al máximo.
- Comprobar que en pantalla angosta no aparece ningún control que modifique datos.
- Las pruebas del servidor (`rpi/tests`) no cambian; no se espera que fallen.

## 13. Lo que queda al implementador

Espaciados exactos, radios, sombras, curvas de animación, iconografía (símbolos simples en SVG propio o texto), estructura
interna de archivos de `web2/` y gráficas dibujadas con SVG o canvas sin librerías.

## 14. Pendientes que no decide este documento

- **Red (decidido):** la Raspberry Pi crea su propia red Wi-Fi (punto de acceso) y el teléfono y la laptop se conectan a ella; el panel se abre en la dirección de la Pi (`http://<ip-de-la-pi>:8080`). Consecuencias para el diseño: sin internet en esa red (por eso no hay fuentes ni librerías externas) y conviene mostrar la dirección de acceso en el panel. Configurar el punto de acceso en la Pi queda fuera de este documento (tarea del módulo D del plan).
- Este diseño no se ha probado en el hardware real ni en la Raspberry Pi (igual que el resto del sistema).

## 15. Ajustes hechos durante la implementación

Decididos al construir (el plan los registra como rulings); el resto del documento no cambia.

- **Orden de la banda:** entrada → sensor de inspección → lector → **sensor de salida** → compuerta → contenedores, según `docs/maquina-de-estados.md` §4 (§6.3 omitía el sensor de salida).
- **Tema:** `?tema=oscuro|claro` lo fuerza (para proyectar); sin él se sigue el modo del sistema. No hay interruptor visible.
- **Compuerta:** no muestra destino mientras clasifica (el estado del ESP32 no lo informa); el contenedor destino se resalta 2.5 s cuando llega la pieza.
- **Carga y errores:** "Conectando con la Pi…" mientras no hay datos; indicación de qué hacer en `ERROR`, `EMERGENCIA`, `PAUSA` y `PAUSA_CONTENEDOR`.
- **Pestañas:** botones de navegación con `aria-current="page"` (sin roles de pestaña incompletos).
- **Parámetros del demo:** `?velocidad`, `?falla`, `?previo` y `?vista` (ver `web2/README.md`).
- **Última pieza y Resumen (cambio posterior a la primera versión):** los diagramas de §6 muestran la rejilla de cifras original. Se sustituyó por un ticket con franja del color del destino y línea de las últimas 12 piezas, más un resumen con total, ciclo promedio y barra apilada A / B / Rechazo con porcentajes. Elegido por la persona entre tres propuestas (combinación de "ticket + barra" y "línea de piezas recientes").
