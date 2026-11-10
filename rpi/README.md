# Raspberry Pi — supervisor, base de datos y panel

`servidor.py` lee la UART del ESP32, guarda piezas y alertas en SQLite, responde ACK/VIVO/SYNC y sirve el
panel web (`../web`) con datos en vivo. Protocolo: [docs/protocolo-uart.md](../docs/protocolo-uart.md).

> **Estado:** probado en la PC con pruebas automáticas (37 pruebas) y con el ESP32 emulado
> (`emulador_esp32.py`), incluida una pérdida del 30 % de tramas. **Todavía no se ha ejecutado en la
> Raspberry Pi ni con el ESP32 real.**

## 1. Requisitos

- Python 3.9 o más reciente (Raspberry Pi OS ya lo trae) — solo biblioteca estándar.
- pyserial: `sudo apt install python3-serial`.
- UART habilitada: `sudo raspi-config` → *Interface Options* → *Serial Port* →
  "login shell over serial?" **No** → "serial port hardware enabled?" **Sí**. Reinicia.
  Comprueba que existe `/dev/serial0` (`ls -l /dev/serial0`).
- El usuario debe pertenecer al grupo `dialout` (`groups`; si no: `sudo usermod -aG dialout $USER` y vuelve a entrar).

Cableado ESP32 ↔ Pi: [docs/conexiones.md](../docs/conexiones.md) §4.

## 2. Ejecutar

```bash
cd rpi
python3 servidor.py                  # /dev/serial0, panel en http://<ip-de-la-pi>:8080
```

| Opción | Por defecto | Uso |
|---|---|---|
| `--puerto` | `/dev/serial0` | Puerto serie del ESP32 (`/dev/ttyUSB0` si se conecta por USB) |
| `--baud` | 115200 | |
| `--sin-uart` | — | Solo panel y base de datos, sin abrir el puerto (no necesita pyserial) |
| `--host` | `0.0.0.0` | Dirección donde escucha el panel (`127.0.0.1` = solo la propia Pi) |
| `--http` | 8080 | Puerto del panel |
| `--db` | `rpi/banda.db` | Archivo SQLite |
| `--web` | `../web` | Carpeta del panel |
| `-v` | — | Muestra también LATIDO/VIVO y las peticiones HTTP |

Si el puerto no existe o se desconecta, el servidor lo reintenta cada 2 s sin cerrarse.

Abre `http://<ip-de-la-pi>:8080` desde cualquier equipo de la misma red. El panel detecta el servidor y pasa
a **modo UART** (datos reales); sin servidor funciona como simulador ([web/README.md](../web/README.md)).

### Arranque automático (opcional)

`banda.service` es una unidad de systemd de ejemplo. Revisa el usuario y la ruta (`User=`, `WorkingDirectory=`)
antes de instalarla:

```bash
sudo cp banda.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now banda
journalctl -u banda -f              # ver el registro
```

## 3. Trabajar sin el ESP32: emulador

```bash
python3 emulador_esp32.py                        # imprime un puerto virtual /dev/pts/N
python3 servidor.py --puerto /dev/pts/N          # en otra terminal
```

Opciones: `--intervalo 4` (segundos entre piezas), `--perder 0.3` (probabilidad de perder cada `PIEZA`, para
probar reenvíos). Teclas (escribir y Enter): `c` enlace caído 10 s, `e` emergencia, `r` restablecer,
`n` pieza sin etiqueta, `u` etiqueta no registrada, `p` pausar, `s` estado.

Las etiquetas del emulador son **inventadas** (las mismas del simulador del panel). Regístralas en el panel para
que vayan a A o B. El emulador **no** reemplaza las pruebas con el hardware: no tiene sensores, motor ni NFC.

## 4. API HTTP (la usa el panel)

| Método y ruta | Uso |
|---|---|
| `GET /api/estado` | Instantánea: estado, enlace, contenedores, etiquetas, últimas piezas y alertas, estadísticas UART |
| `GET /api/eventos` | Eventos en vivo (Server-Sent Events) |
| `GET /api/historial.csv` | Historial completo para el reporte |
| `POST /api/comando` | `{"cmd":"PARO"}`, `{"cmd":"VACIADO","categoria":"A"}`, `{"cmd":"CAPACIDAD","categoria":"A","capacidad":20}`, `{"cmd":"SYNC"}` |
| `POST /api/etiquetas` | `{"uid":"04A31FB26C8000","nombre":"…","categoria":"A"}` — envía `SYNC` al ESP32 |
| `DELETE /api/etiquetas/<uid>` | Elimina y envía `SYNC` |
| `POST /api/alertas/reconocer` | Marca todas las alertas como reconocidas |

No hay usuarios ni contraseñas: cualquiera en la red puede usar el panel. Úsalo solo en una red de confianza.

## 5. Base de datos

`banda.db` (SQLite). Tablas: `etiquetas` (tabla maestra), `piezas` (clave única `epoca, seq`; columna `tardio`
para las que llegaron después de una desconexión), `alertas`, `contenedores`, `config` (versión de la tabla y
época). Para consultarla: `sqlite3 banda.db "SELECT * FROM piezas ORDER BY id DESC LIMIT 10;"`.
El archivo no se sube al repositorio (`.gitignore`).

## 6. Estructura y pruebas

| Archivo | Contenido |
|---|---|
| `servidor.py` | Programa principal: argumentos, puerto serie, hilos |
| `banda/protocolo.py` | Checksum, tramas, validación de UID |
| `banda/db.py` | SQLite |
| `banda/supervisor.py` | Lógica: ACK, duplicados, huecos, SYNC, épocas, enlace, eventos para el panel |
| `banda/servidor.py` | HTTP y SSE |
| `emulador_esp32.py` | ESP32 emulado |
| `tests/` | Pruebas unitarias e integración (supervisor + emulador en memoria, HTTP real en un puerto local) |

```bash
cd rpi
python3 -m unittest discover -s tests -t . -v
```
No necesitan pyserial ni hardware.
