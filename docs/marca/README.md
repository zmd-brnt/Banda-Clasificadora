# Triar — marca del proyecto

**Triar** es el nombre del sistema (banda clasificadora con lectura NFC) y de su panel web. "Triar" significa elegir y separar, que es lo que hace la banda: cada pieza entra, el lector la identifica y sale por una de tres vías (A, B o rechazo).

> **Estado:** los nombres Triar, Cerne y Tamiz son palabras comunes del español. **No se revisó ningún registro de
> marca ni buscador**; sirve para el proyecto escolar, pero habría que buscarlo antes de un uso fuera de la clase.

## El símbolo

Una línea entra por un anillo (el lector NFC) y se bifurca en tres salidas. La del medio es más corta: es el rechazo. Trazos redondeados de un solo grosor, sin degradados ni efectos.

## Archivos

| Archivo | Uso |
|---|---|
| `simbolo.svg` | Símbolo maestro (tamaños desde 24 px) |
| `simbolo-pequeno.svg` | Versión más gruesa para **16 a 24 px** (pestaña del navegador, iconos) |
| `horizontal.svg` | Símbolo + nombre en una línea (encabezado del panel, README) |
| `vertical.svg` | Símbolo sobre el nombre (portada, diapositiva) |
| `export-simbolo/`, `export-horizontal/`, `export-vertical/` | Variantes: negro, blanco, un solo color (`#00766E`), cuadrada |
| `export-simbolo/simbolo-favicon.svg`, `simbolo-app-icon.svg` | Favicon e icono de aplicación: símbolo cian sobre mosaico oscuro |
| `export-simbolo/icono-32.png`, `-180.png`, `-192.png`, `-512.png` | Iconos PNG con esquinas transparentes (180 = pantalla de inicio de iOS; 192 y 512 = Android) |

Los SVG usan `fill="#000"` en negro: para cambiar el color se reemplaza ese valor (las variantes ya lo hacen).

## Color

| Rol | Valor | Contraste |
|---|---|---|
| Cian Triar (sobre oscuro) | `#38D6C8` sobre `#0E1417` | 10.3 : 1 |
| Cian Triar (sobre claro) | `#00766E` sobre `#FFFFFF` | 5.5 : 1 |
| Fondo oscuro | `#0E1417` | — |

El cian de la marca es el mismo del panel (ver [diseño del panel](../superpowers/specs/2026-11-15-panel-nuevo-design.md) §6.1). En una sola tinta se usa negro, blanco o `#00766E`. No se calculó ningún valor CMYK ni Pantone.

## Uso

- **Espacio libre:** como mínimo, el diámetro exterior del anillo alrededor del símbolo (criterio propio, no medido en pruebas de impresión).
- **Tamaño mínimo:** símbolo 24 px; de 16 a 24 px usar `simbolo-pequeno.svg`; logo horizontal, 120 px de ancho.
- **Fondos:** sobre `#0E1417` en cian `#38D6C8`; sobre blanco en `#00766E` o negro. Sobre fotos o colores medios, en blanco o negro con buen contraste.
- **No hacer:** estirar o girar el símbolo, cambiar el grosor, añadir sombras o degradados, rellenar el anillo, usar cian claro sobre fondo claro, sustituir el nombre por otra fuente.

## Limitaciones

- Los SVG son de **trazos** (no contornos expandidos): se ven igual en pantalla, pero un impresor o un grabado láser pedirá los trazos convertidos a contornos (Inkscape: *Trazo a contorno*).
- El nombre "Triar" está **construido con trazos geométricos**, no es ninguna fuente; es una primera versión que conviene refinar a ojo si va a usarse en impresión grande.
- No se generó `favicon.ico` (no había herramienta para ello); los navegadores modernos usan el `.svg` o el `.png`.
- Se comprobó el símbolo a 200, 64, 32 y 16 px en pantalla. **No** se hizo la presentación con maquetas (taza, tarjeta, etc.) del kit completo.
