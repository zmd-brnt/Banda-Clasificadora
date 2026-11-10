"""Servidor HTTP del panel (solo biblioteca estandar).

Rutas:
  GET  /                     panel web (archivos de la carpeta web/)
  GET  /api/estado           instantanea completa en JSON (al cargar la pagina)
  GET  /api/eventos          eventos en vivo (Server-Sent Events)
  GET  /api/historial.csv    historial completo desde la base de datos
  POST /api/comando          {"cmd": "SYNC" | "VACIADO" | "CAPACIDAD" | "PARO", ...}
  POST /api/etiquetas        {"uid", "nombre", "categoria"}   registra o actualiza
  DELETE /api/etiquetas/UID  elimina
  POST /api/alertas/reconocer
"""

import csv
import io
import json
import logging
import mimetypes
import os
import queue
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import unquote, urlparse

from . import protocolo

log = logging.getLogger("banda.servidor")

KEEPALIVE_S = 15
MAX_CUERPO = 64 * 1024


def crear_servidor(direccion, puerto, supervisor, carpeta_web):
    carpeta_web = os.path.realpath(carpeta_web)

    class Manejador(BaseHTTPRequestHandler):
        server_version = "BandaClasificadora/1.0"
        protocol_version = "HTTP/1.1"

        def log_message(self, formato, *args):
            log.debug("%s - %s", self.address_string(), formato % args)

        # -------------------------------------------------------------- utilidades
        def _json(self, codigo, datos):
            cuerpo = json.dumps(datos, ensure_ascii=False).encode("utf-8")
            self.send_response(codigo)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(cuerpo)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(cuerpo)

        def _leer_json(self):
            largo = int(self.headers.get("Content-Length") or 0)
            if largo > MAX_CUERPO:
                raise ValueError("cuerpo demasiado grande")
            datos = json.loads(self.rfile.read(largo) or b"{}")
            if not isinstance(datos, dict):
                raise ValueError("se esperaba un objeto JSON")
            return datos

        def _archivo(self, ruta):
            relativa = unquote(ruta).lstrip("/") or "index.html"
            completa = os.path.realpath(os.path.join(carpeta_web, relativa))
            if not completa.startswith(carpeta_web + os.sep) or not os.path.isfile(completa):
                self._json(404, {"error": "no encontrado"})
                return
            tipo = mimetypes.guess_type(completa)[0] or "application/octet-stream"
            if tipo.startswith("text/") or tipo in ("application/javascript",):
                tipo += "; charset=utf-8"
            with open(completa, "rb") as f:
                contenido = f.read()
            self.send_response(200)
            self.send_header("Content-Type", tipo)
            self.send_header("Content-Length", str(len(contenido)))
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            self.wfile.write(contenido)

        def _eventos(self):
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Connection", "keep-alive")
            self.end_headers()
            q = supervisor.suscribir()
            try:
                self.wfile.write(b": conectado\n\n")
                self.wfile.flush()
                while True:
                    try:
                        evento = q.get(timeout=KEEPALIVE_S)
                        dato = json.dumps(evento, ensure_ascii=False)
                        self.wfile.write(f"data: {dato}\n\n".encode("utf-8"))
                    except queue.Empty:
                        self.wfile.write(b": ping\n\n")   # mantiene viva la conexion
                    self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
            finally:
                supervisor.desuscribir(q)
                self.close_connection = True

        def _csv(self):
            salida = io.StringIO()
            w = csv.writer(salida)
            w.writerow(["epoca", "seq", "fecha_hora", "uid", "objeto", "categoria", "motivo", "ciclo_ms", "tardio"])
            for p in supervisor.db.piezas(limite=1_000_000):
                w.writerow([p["epoca"], p["seq"], datetime.fromtimestamp(p["ts"] / 1000).isoformat(timespec="seconds"),
                            p["uid"] or "", p["nombre"] or "", p["categoria"], p["motivo"] or "", p["ciclo_ms"],
                            p["tardio"]])
            cuerpo = ("﻿" + salida.getvalue()).encode("utf-8")
            nombre = datetime.now().strftime("historial_banda_%Y-%m-%d_%H-%M-%S.csv")
            self.send_response(200)
            self.send_header("Content-Type", "text/csv; charset=utf-8")
            self.send_header("Content-Disposition", f'attachment; filename="{nombre}"')
            self.send_header("Content-Length", str(len(cuerpo)))
            self.end_headers()
            self.wfile.write(cuerpo)

        # -------------------------------------------------------------- metodos
        def do_GET(self):
            ruta = urlparse(self.path).path
            if ruta == "/api/estado":
                self._json(200, supervisor.instantanea())
            elif ruta == "/api/eventos":
                self._eventos()
            elif ruta == "/api/historial.csv":
                self._csv()
            elif ruta.startswith("/api/"):
                self._json(404, {"error": "ruta desconocida"})
            else:
                self._archivo(ruta)

        def do_POST(self):
            ruta = urlparse(self.path).path
            try:
                datos = self._leer_json()
            except (ValueError, json.JSONDecodeError) as e:
                self._json(400, {"ok": False, "error": f"JSON invalido: {e}"})
                return

            if ruta == "/api/comando":
                ok, mensaje = supervisor.comando(datos)
                self._json(200 if ok else 400, {"ok": ok, "mensaje": mensaje})
            elif ruta == "/api/etiquetas":
                try:
                    v = supervisor.db.registrar_etiqueta(datos.get("uid", ""), datos.get("nombre", ""),
                                                         datos.get("categoria"))
                except protocolo.ErrorProtocolo as e:
                    self._json(400, {"ok": False, "error": str(e)})
                    return
                supervisor.etiquetas_cambiaron()
                supervisor.sincronizar()
                self._json(200, {"ok": True, "version": v})
            elif ruta == "/api/alertas/reconocer":
                supervisor.db.reconocer_alertas()
                self._json(200, {"ok": True})
            else:
                self._json(404, {"ok": False, "error": "ruta desconocida"})

        def do_DELETE(self):
            ruta = urlparse(self.path).path
            prefijo = "/api/etiquetas/"
            if not ruta.startswith(prefijo):
                self._json(404, {"ok": False, "error": "ruta desconocida"})
                return
            try:
                v = supervisor.db.eliminar_etiqueta(unquote(ruta[len(prefijo):]))
            except protocolo.ErrorProtocolo as e:
                self._json(400, {"ok": False, "error": str(e)})
                return
            if v is None:
                self._json(404, {"ok": False, "error": "esa etiqueta no existe"})
                return
            supervisor.etiquetas_cambiaron()
            supervisor.sincronizar()
            self._json(200, {"ok": True, "version": v})

    servidor = ThreadingHTTPServer((direccion, puerto), Manejador)
    servidor.daemon_threads = True
    return servidor
