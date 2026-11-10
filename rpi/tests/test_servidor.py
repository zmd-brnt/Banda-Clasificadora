"""Pruebas del servidor HTTP: API, eventos en vivo (SSE), archivos del panel y seguridad de rutas."""

import json
import os
import socket
import tempfile
import threading
import unittest
import urllib.error
import urllib.request

from banda import protocolo
from banda.db import BaseDatos
from banda.servidor import crear_servidor
from banda.supervisor import Supervisor

WEB = os.path.join(os.path.dirname(__file__), "..", "..", "web")


class TestServidor(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.db = BaseDatos(os.path.join(self.dir.name, "s.db"))
        self.enviado = []
        self.sup = Supervisor(self.db, self.enviado.append)
        self.srv = crear_servidor("127.0.0.1", 0, self.sup, WEB)
        self.base = f"http://127.0.0.1:{self.srv.server_address[1]}"
        threading.Thread(target=self.srv.serve_forever, daemon=True).start()

    def tearDown(self):
        self.srv.shutdown()
        self.srv.server_close()
        self.db.cerrar()
        self.dir.cleanup()

    def pedir(self, metodo, ruta, datos=None):
        cuerpo = json.dumps(datos).encode() if datos is not None else None
        req = urllib.request.Request(self.base + ruta, data=cuerpo, method=metodo,
                                     headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=5) as r:
                return r.status, r.read()
        except urllib.error.HTTPError as e:
            return e.code, e.read()

    def test_panel_y_estado(self):
        codigo, html = self.pedir("GET", "/")
        self.assertEqual(codigo, 200)
        self.assertIn(b"Banda Clasificadora", html)
        codigo, js = self.pedir("GET", "/js/main.js")
        self.assertEqual(codigo, 200)
        codigo, cuerpo = self.pedir("GET", "/api/estado")
        estado = json.loads(cuerpo)
        self.assertEqual(estado["modo"], "uart")
        self.assertEqual(estado["version_tabla"], 1)

    def test_rutas_fuera_de_web_bloqueadas(self):
        for ruta in ["/../rpi/servidor.py", "/%2e%2e/rpi/servidor.py", "/no-existe.html"]:
            with self.subTest(ruta=ruta):
                codigo, _ = self.pedir("GET", ruta)
                self.assertEqual(codigo, 404)

    def test_registrar_y_eliminar_etiqueta(self):
        codigo, cuerpo = self.pedir("POST", "/api/etiquetas", {"uid": "04:aa:bb", "nombre": "Tuerca", "categoria": "B"})
        self.assertEqual(codigo, 200, cuerpo)
        self.assertEqual(json.loads(cuerpo)["version"], 2)
        self.assertIn(protocolo.trama("SYNC,2,04AABB=B"), self.enviado)   # se sincroniza de inmediato
        codigo, _ = self.pedir("POST", "/api/etiquetas", {"uid": "xyz", "nombre": "a", "categoria": "A"})
        self.assertEqual(codigo, 400)
        codigo, _ = self.pedir("DELETE", "/api/etiquetas/04AABB")
        self.assertEqual(codigo, 200)
        codigo, _ = self.pedir("DELETE", "/api/etiquetas/04AABB")
        self.assertEqual(codigo, 404)

    def test_comando(self):
        codigo, cuerpo = self.pedir("POST", "/api/comando", {"cmd": "VACIADO", "categoria": "A"})
        self.assertEqual(codigo, 200)
        self.assertIn(protocolo.trama("VACIADO,A"), self.enviado)
        codigo, _ = self.pedir("POST", "/api/comando", {"cmd": "VACIADO", "categoria": "Z"})
        self.assertEqual(codigo, 400)
        req = urllib.request.Request(self.base + "/api/comando", data=b"no es json", method="POST")
        with self.assertRaises(urllib.error.HTTPError) as e:
            urllib.request.urlopen(req, timeout=5)
        self.assertEqual(e.exception.code, 400)

    def test_eventos_sse(self):
        puerto = self.srv.server_address[1]
        s = socket.create_connection(("127.0.0.1", puerto), timeout=5)
        s.sendall(b"GET /api/eventos HTTP/1.1\r\nHost: x\r\n\r\n")
        recibido = b""
        while b": conectado" not in recibido:
            recibido += s.recv(4096)
        self.sup.procesar_linea(protocolo.trama("PIEZA,1,04AA,A,OK,3000,0").decode())
        while b'"tipo": "PIEZA"' not in recibido:
            recibido += s.recv(4096)
        s.close()
        self.assertIn(b"text/event-stream", recibido)
        self.assertIn(protocolo.trama("ACK,1"), self.enviado)

    def test_csv(self):
        self.sup.procesar_linea(protocolo.trama("PIEZA,1,-,R,SIN_LECTURA,1500,0").decode())
        codigo, cuerpo = self.pedir("GET", "/api/historial.csv")
        self.assertEqual(codigo, 200)
        texto = cuerpo.decode("utf-8-sig").splitlines()
        self.assertEqual(texto[0].split(",")[1], "seq")
        self.assertIn("SIN_LECTURA", texto[1])


if __name__ == "__main__":
    unittest.main()
