# Triar — panel web nuevo

Panel de supervisión y operación de la banda clasificadora. Reemplaza al panel de simulación de [`web/`](../web/README.md), que se conserva intacto como respaldo y **no comparte código** con este. Diseño: [spec](../docs/superpowers/specs/2026-11-15-panel-nuevo-design.md). Marca: [`docs/marca/`](../docs/marca/README.md).

> **Estado:** probado con pruebas automáticas, con el ESP32 emulado ([`rpi/emulador_esp32.py`](../rpi/README.md)) y en un navegador simulado (jsdom) y Firefox. **No se ha probado con el ESP32 real, en la Raspberry Pi ni con el punto de acceso Wi-Fi de la Pi.**

## 1. Cómo abrirlo

No necesita instalar nada ni conexión a internet: no usa librerías, fuentes ni imágenes externas.

**Con datos reales** (en la Raspberry Pi, ver [rpi/README.md](../rpi/README.md)):

```bash
cd rpi
python3 servidor.py --web ../web2     # abrir http://<ip-de-la-pi>:8080
```

El teléfono y la laptop se conectan a la red Wi-Fi que crea la Pi y abren esa dirección. El pie de la página muestra la dirección a la que se está conectado.

**Modo demostración** (sin ESP32 ni Pi): abre `web2/index.html` con doble clic, o sirve la carpeta con `cd web2 && python3 -m http.server 8000`. Se activa solo cuando no hay servidor de la Pi, o con `?demo` en la dirección. Una franja avisa "Modo demostración — datos inventados"; ningún valor es un resultado experimental.

### Parámetros de la dirección

| Parámetro | Efecto |
| --- | --- |
| `?demo` | Fuerza el modo demostración |
| `?tema=oscuro` o `?tema=claro` | Fuerza el tema (sin él se sigue el modo del sistema; útil para proyectar) |
| `?vista=inventario` | Abre esa sección (`tiempo-real`, `inventario`, `etiquetas`, `historial`, `alertas`) |
| `?velocidad=8` | Acelera el demo (para ensayos) |
| `?falla=ATASCO` | Provoca una falla en el demo: `SIN_ETIQUETA`, `NO_REGISTRADA`, `ATASCO`, `ENLACE_CAIDO` o `CONTENEDOR_LLENO` |
| `?previo=40` | Adelanta N pasos del demo al abrir (para capturas) |

## 2. Qué muestra en cada dispositivo

El panel decide por el ancho de la pantalla: **900 px o más** es operación (laptop) y menos es **solo supervisión** (teléfono). No es seguridad: una tablet, o un teléfono con "sitio de escritorio", mostraría los controles; el panel tampoco tiene usuarios ni contraseñas (ver [rpi/README.md](../rpi/README.md) §4).

| Sección | Laptop | Teléfono |
| --- | --- | --- |
| Tiempo real | Banda horizontal, última pieza con piezas recientes, resumen con reparto, gráficas, eventos | Banda vertical, última pieza con piezas recientes, resumen con reparto, 5 eventos |
| Inventario | Ocupación, **Marcar vaciado**, **Cambiar capacidad**, tabla por objeto | Solo lectura |
| Etiquetas | Registrar, eliminar, **Sincronizar con ESP32**, UID sin registrar | No aparece |
| Historial | Tabla de hasta 200 piezas, filtro por destino, **Exportar CSV** | Últimas 20 piezas |
| Alertas | Lista y **Reconocer todas** | Solo lectura |
| Paro | **PARO** siempre visible, sin confirmación (paro por software; el seguro es el botón físico) | No aparece |

En Tiempo real, la **última pieza** es un ticket con una franja del color de su destino y, debajo, las últimas 12 piezas como insignias A / B / R (la más reciente resaltada). El **resumen** muestra el total, el ciclo promedio y una barra apilada A / B / Rechazo con cantidad y porcentaje; la barra y la línea de recientes tienen texto alternativo para lectores de pantalla.

Arrancar y restablecer la banda solo se hacen con el botón físico. La animación de la banda sigue el **estado** del ESP32, no la posición real de la pieza.

## 3. Estructura

```text
web2/
├── index.html        # armazón y carga de scripts
├── css/              # tokens (colores), base, layout (responsivo) y banda
├── js/
│   ├── formato.js    # texto seguro, horas, nombres de estados
│   ├── modo.js       # modo según ancho; acciones permitidas
│   ├── modelo.js     # almacén de datos y selectores
│   ├── fuente-real.js, fuente-demo.js   # de dónde vienen los datos y los comandos
│   ├── etapas.js, graficas.js           # estado → etapa de la banda; gráficas SVG
│   ├── vistas.js     # funciones puras que devuelven HTML
│   └── app.js        # único archivo que toca el DOM
├── img/              # favicon e iconos (copiados de docs/marca)
└── tests/            # pruebas con node:test
```

Los datos entran por el modelo (lo llena la fuente real o la demo) y todas las vistas leen de él, así que real y demo comparten la interfaz. Los mensajes y estados son los del servidor: [web/README.md §5](../web/README.md) y [docs/protocolo-uart.md](../docs/protocolo-uart.md).

## 4. Pruebas

Desde la raíz del repositorio, con Node 20 o más reciente (no necesita instalar paquetes):

```bash
node --test "web2/tests/*.test.js"
```

Cubren el formato, el modelo, el cliente de la Pi (con `fetch` y `EventSource` falsos), el modo demo, los contrastes de color y que en supervisión ninguna vista tenga controles. Las comprobaciones del navegador (clics, cambio de tamaño, servidor real) se hicieron aparte con jsdom y no están en el repositorio.

## 5. Qué es real y qué es supuesto

| Elemento | Origen |
| --- | --- |
| Datos, estados y comandos | La API de la Pi ([rpi/README.md](../rpi/README.md) §4) |
| Colores y su contraste | Spec §6.1; comprobados con `tests/contraste.test.js` |
| Datos del modo demo (etiquetas, tiempos, 15 piezas de capacidad) | **Inventados** |
| Umbral de 900 px | Decisión de diseño |
