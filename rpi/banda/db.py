"""Base de datos SQLite del supervisor.

Tablas:
  etiquetas     uid (PK), nombre, categoria, registrada            -> tabla maestra
  piezas        epoca + seq (unicos), ts, uid, categoria, motivo,   -> historial
                ciclo_ms, reenvio, recibido
  alertas       id, ts, nivel, codigo, mensaje, reconocida
  contenedores  categoria (PK), contenido, capacidad                -> copia de lo que reporta el ESP32
  config        clave (PK), valor                                   -> version_tabla, epoca

"epoca": si el ESP32 pierde su memoria (NVS borrada), su numeracion vuelve a empezar en 1.
Para no confundir esas piezas con duplicados, la Pi abre una epoca nueva y la clave unica
pasa a ser (epoca, seq).
"""

import sqlite3
import threading
import time

from . import protocolo

ESQUEMA = """
CREATE TABLE IF NOT EXISTS etiquetas (
    uid        TEXT PRIMARY KEY,
    nombre     TEXT NOT NULL,
    categoria  TEXT NOT NULL CHECK (categoria IN ('A','B','R')),
    registrada INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS piezas (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    epoca     INTEGER NOT NULL,
    seq       INTEGER NOT NULL,
    ts        INTEGER NOT NULL,
    uid       TEXT,
    categoria TEXT NOT NULL,
    motivo    TEXT,
    ciclo_ms  INTEGER NOT NULL,
    reenvio   INTEGER NOT NULL,
    tardio    INTEGER NOT NULL DEFAULT 0,
    recibido  INTEGER NOT NULL,
    UNIQUE (epoca, seq)
);
CREATE TABLE IF NOT EXISTS alertas (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    ts         INTEGER NOT NULL,
    nivel      TEXT NOT NULL,
    codigo     TEXT NOT NULL,
    mensaje    TEXT NOT NULL,
    reconocida INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS contenedores (
    categoria TEXT PRIMARY KEY,
    contenido INTEGER NOT NULL,
    capacidad INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS config (
    clave TEXT PRIMARY KEY,
    valor TEXT NOT NULL
);
"""

CAPACIDAD_INICIAL = 15   # mismo valor que CAPACIDAD_INICIAL del firmware


def ahora_ms():
    return int(time.time() * 1000)


class BaseDatos:
    def __init__(self, ruta):
        self._con = sqlite3.connect(ruta, check_same_thread=False)
        self._con.row_factory = sqlite3.Row
        self._lock = threading.RLock()
        with self._lock, self._con:
            self._con.executescript(ESQUEMA)
            self._con.execute("INSERT OR IGNORE INTO config VALUES ('version_tabla', '1')")
            self._con.execute("INSERT OR IGNORE INTO config VALUES ('epoca', '1')")
            for cat in protocolo.CATEGORIAS:
                self._con.execute("INSERT OR IGNORE INTO contenedores VALUES (?, 0, ?)", (cat, CAPACIDAD_INICIAL))

    def cerrar(self):
        with self._lock:
            self._con.close()

    # ------------------------------------------------------------------ config
    def _get(self, clave):
        return int(self._con.execute("SELECT valor FROM config WHERE clave = ?", (clave,)).fetchone()[0])

    def _set(self, clave, valor):
        self._con.execute("UPDATE config SET valor = ? WHERE clave = ?", (str(valor), clave))

    def version_tabla(self):
        with self._lock:
            return self._get("version_tabla")

    def epoca(self):
        with self._lock:
            return self._get("epoca")

    def nueva_epoca(self):
        with self._lock, self._con:
            e = self._get("epoca") + 1
            self._set("epoca", e)
            return e

    # ------------------------------------------------------------------ etiquetas
    def etiquetas(self):
        with self._lock:
            filas = self._con.execute("SELECT uid, nombre, categoria, registrada FROM etiquetas ORDER BY uid")
            return [dict(f) for f in filas]

    def tabla_sync(self):
        """dict uid -> categoria para el comando SYNC."""
        with self._lock:
            return {f["uid"]: f["categoria"] for f in self._con.execute("SELECT uid, categoria FROM etiquetas")}

    def registrar_etiqueta(self, uid, nombre, categoria):
        """Crea o actualiza una etiqueta. Devuelve la nueva version de la tabla.

        Lanza protocolo.ErrorProtocolo si el dato es invalido o si la tabla ya no cabe en el ESP32.
        """
        uid = protocolo.normalizar_uid(uid)
        nombre = (nombre or "").strip()
        if not nombre or len(nombre) > 60:
            raise protocolo.ErrorProtocolo("el nombre es obligatorio (maximo 60 caracteres)")
        if categoria not in ("A", "B", "R"):
            raise protocolo.ErrorProtocolo("la categoria debe ser A, B o R")
        with self._lock, self._con:
            tabla = self.tabla_sync()
            tabla[uid] = categoria
            protocolo.cmd_sync(self._get("version_tabla") + 1, tabla)   # valida tamano y longitud
            self._con.execute(
                "INSERT INTO etiquetas VALUES (?, ?, ?, ?) "
                "ON CONFLICT(uid) DO UPDATE SET nombre = excluded.nombre, categoria = excluded.categoria",
                (uid, nombre, categoria, ahora_ms()))
            v = self._get("version_tabla") + 1
            self._set("version_tabla", v)
            return v

    def eliminar_etiqueta(self, uid):
        uid = protocolo.normalizar_uid(uid)
        with self._lock, self._con:
            if self._con.execute("DELETE FROM etiquetas WHERE uid = ?", (uid,)).rowcount == 0:
                return None
            v = self._get("version_tabla") + 1
            self._set("version_tabla", v)
            return v

    def nombre_de(self, uid):
        if not uid:
            return None
        with self._lock:
            f = self._con.execute("SELECT nombre FROM etiquetas WHERE uid = ?", (uid,)).fetchone()
            return f["nombre"] if f else None

    # ------------------------------------------------------------------ piezas
    def existe_pieza(self, epoca, seq):
        with self._lock:
            return self._con.execute("SELECT 1 FROM piezas WHERE epoca = ? AND seq = ?", (epoca, seq)).fetchone() is not None

    def max_seq(self, epoca):
        with self._lock:
            return self._con.execute("SELECT COALESCE(MAX(seq), 0) FROM piezas WHERE epoca = ?", (epoca,)).fetchone()[0]

    def faltantes(self, epoca, hasta):
        """seq entre 1 y hasta-1 que no estan registrados en la epoca."""
        with self._lock:
            vistos = {f[0] for f in self._con.execute("SELECT seq FROM piezas WHERE epoca = ? AND seq < ?", (epoca, hasta))}
        return [s for s in range(1, hasta) if s not in vistos]

    def guardar_pieza(self, epoca, p, ts, tardio):
        """Devuelve True si la pieza era nueva (False si ya existia)."""
        with self._lock, self._con:
            try:
                self._con.execute(
                    "INSERT INTO piezas (epoca, seq, ts, uid, categoria, motivo, ciclo_ms, reenvio, tardio, recibido) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                    (epoca, p["seq"], ts, p["uid"], p["categoria"], p["motivo"], p["ciclo_ms"],
                     int(p["reenvio"]), int(tardio), ahora_ms()))
                return True
            except sqlite3.IntegrityError:
                return False

    def piezas(self, limite=5000):
        with self._lock:
            filas = self._con.execute(
                "SELECT p.epoca, p.seq, p.ts, p.uid, p.categoria, p.motivo, p.ciclo_ms, p.reenvio, p.tardio, "
                "e.nombre FROM piezas p LEFT JOIN etiquetas e ON e.uid = p.uid "
                "ORDER BY p.epoca DESC, p.seq DESC LIMIT ?", (limite,))
            return [dict(f) for f in filas][::-1]

    def conteos(self):
        with self._lock:
            c = {"A": 0, "B": 0, "R": 0, "SIN_LECTURA": 0, "NO_REGISTRADA": 0}
            for f in self._con.execute("SELECT categoria, COUNT(*) FROM piezas GROUP BY categoria"):
                c[f[0]] = f[1]
            for f in self._con.execute("SELECT motivo, COUNT(*) FROM piezas WHERE motivo IS NOT NULL GROUP BY motivo"):
                c[f[0]] = f[1]
            return c

    # ------------------------------------------------------------------ alertas
    def guardar_alerta(self, nivel, codigo, mensaje, ts=None):
        ts = ts or ahora_ms()
        with self._lock, self._con:
            cur = self._con.execute("INSERT INTO alertas (ts, nivel, codigo, mensaje) VALUES (?, ?, ?, ?)",
                                    (ts, nivel, codigo, mensaje))
            return {"id": cur.lastrowid, "ts": ts, "nivel": nivel, "codigo": codigo, "mensaje": mensaje,
                    "reconocida": False}

    def alertas(self, limite=200):
        with self._lock:
            filas = self._con.execute("SELECT * FROM alertas ORDER BY id DESC LIMIT ?", (limite,))
            return [dict(f, reconocida=bool(f["reconocida"])) for f in filas]

    def reconocer_alertas(self):
        with self._lock, self._con:
            self._con.execute("UPDATE alertas SET reconocida = 1")

    # ------------------------------------------------------------------ contenedores
    def actualizar_contenedores(self, contenido, capacidad):
        with self._lock, self._con:
            for cat in protocolo.CATEGORIAS:
                self._con.execute("UPDATE contenedores SET contenido = ?, capacidad = ? WHERE categoria = ?",
                                  (contenido[cat], capacidad[cat], cat))

    def contenedores(self):
        with self._lock:
            return {f["categoria"]: {"contenido": f["contenido"], "capacidad": f["capacidad"]}
                    for f in self._con.execute("SELECT * FROM contenedores")}
