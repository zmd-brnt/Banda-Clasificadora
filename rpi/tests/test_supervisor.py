"""Pruebas del supervisor conectado al ESP32 emulado (sin hardware ni puerto serie).

Reproducen en software los casos 9 a 13 de pruebas/integracion_test/README.md: ACK perdido,
Pi desconectada, reinicio con memoria borrada, tabla nueva y estabilidad.
"""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from banda.db import BaseDatos                     # noqa: E402
from banda.supervisor import Supervisor            # noqa: E402
from emulador_esp32 import EmuladorEsp32           # noqa: E402


class Banco:
    """Supervisor y emulador conectados por dos colas en memoria."""

    def __init__(self, ruta_db, perder=0.0):
        self.a_pi = []
        self.a_esp = []
        self.db = BaseDatos(ruta_db)
        self.sup = Supervisor(self.db, self.a_esp.append)
        self.emu = EmuladorEsp32(self.a_pi.append, perder=perder, semilla=1)
        self.t = 0.0
        self.cortar_acks = 0            # descarta los siguientes N ACK de la Pi
        self.eventos = self.sup.suscribir()

    def paso(self, dt=0.1):
        """Avanza el reloj y entrega todo lo pendiente en ambos sentidos."""
        self.t += dt
        self.emu.tick(self.t)
        for _ in range(10):
            if not self.a_pi and not self.a_esp:
                break
            while self.a_pi:
                self.sup.procesar_linea(self.a_pi.pop(0).decode(), self.t)
            while self.a_esp:
                linea = self.a_esp.pop(0).decode()
                if linea.startswith("ACK,") and self.cortar_acks > 0:
                    self.cortar_acks -= 1
                    continue
                self.emu.recibir(linea, self.t)
        self.sup.revisar_enlace(self.t)

    def correr(self, segundos):
        for _ in range(int(segundos / 0.1)):
            self.paso()

    def tipos_de_eventos(self):
        tipos = []
        while not self.eventos.empty():
            tipos.append(self.eventos.get_nowait())
        return tipos


class TestSupervisor(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.b = Banco(os.path.join(self.dir.name, "s.db"))
        self.b.correr(1.5)   # primer latido -> VIVO -> enlace activo

    def tearDown(self):
        self.b.db.cerrar()
        self.dir.cleanup()

    def seqs(self):
        return [p["seq"] for p in self.b.db.piezas()]

    def test_enlace_y_sync_automatico(self):
        self.assertTrue(self.b.sup.enlace_ok)
        self.assertTrue(self.b.emu.enlace)
        self.b.db.registrar_etiqueta("04A31FB26C8000", "Tornillo", "A")
        self.b.correr(2.5)
        self.assertEqual(self.b.emu.version, self.b.db.version_tabla())   # caso 12: tabla nueva
        self.assertEqual(self.b.emu.tabla, {"04A31FB26C8000": "A"})
        self.b.emu.pieza("04A31FB26C8000", self.b.t)
        self.b.correr(0.5)
        self.assertEqual(self.b.db.piezas()[-1]["categoria"], "A")

    def test_pieza_registrada_una_vez_y_confirmada(self):
        self.b.emu.pieza(None, self.b.t)
        self.b.emu.pieza("04FFEEDDCCBB00", self.b.t)
        self.b.correr(0.5)
        self.assertEqual(self.seqs(), [1, 2])
        self.assertEqual(self.b.emu.pendientes, {})
        piezas = self.b.db.piezas()
        self.assertEqual(piezas[0]["motivo"], "SIN_LECTURA")
        self.assertEqual(piezas[1]["motivo"], "NO_REGISTRADA")

    def test_ack_perdido_no_duplica(self):   # caso 9
        self.b.cortar_acks = 3
        for _ in range(6):
            self.b.emu.pieza(None, self.b.t)
            self.b.correr(0.3)
        self.b.correr(3)
        self.assertEqual(self.seqs(), [1, 2, 3, 4, 5, 6])
        self.assertGreaterEqual(self.b.sup.stats["duplicados"], 3)
        self.assertEqual(self.b.emu.pendientes, {})

    def test_trama_perdida_se_recupera(self):
        self.b.emu.perder = 1.0               # la primera transmision de cada pieza se pierde
        for _ in range(3):
            self.b.emu.pieza(None, self.b.t)
        self.b.emu.perder = 0.0
        self.b.correr(2)
        self.assertEqual(self.seqs(), [1, 2, 3])

    def test_enlace_caido_y_reconexion(self):   # caso 10
        self.b.emu.caido_hasta = self.b.t + 6
        self.b.correr(4)
        self.assertFalse(self.b.sup.enlace_ok)
        for _ in range(3):
            self.b.emu.pieza(None, self.b.t)   # sigue clasificando sin la Pi
        self.assertEqual(len(self.b.emu.pendientes), 3)
        self.assertEqual(self.seqs(), [])
        self.b.correr(5)
        self.assertTrue(self.b.sup.enlace_ok)
        self.assertEqual(self.seqs(), [1, 2, 3])
        self.assertTrue(all(p["tardio"] for p in self.b.db.piezas()))
        codigos = [a["codigo"] for a in self.b.db.alertas()]
        self.assertIn("ENLACE_CAIDO", codigos)

    def test_hueco_pide_reenvio(self):
        self.b.emu.pieza(None, self.b.t)
        self.b.correr(0.3)
        # El evento 2 se pierde y el ESP32 tampoco lo reintentara (simula buffer desbordado)
        self.b.emu.pieza(None, self.b.t)
        del self.b.emu.pendientes[2]
        self.b.a_pi.clear()
        self.b.emu.pieza(None, self.b.t)
        self.b.correr(1)
        self.assertEqual(self.seqs(), [1, 3])
        self.assertTrue(any(c.startswith("REENVIAR,2") for c in self.b.emu.recibidos))
        codigos = [a["codigo"] for a in self.b.db.alertas()]
        self.assertIn("UART_HUECO", codigos)
        self.assertIn("SEQ_NO_DISPONIBLE", codigos)

    def test_memoria_borrada_nueva_epoca(self):   # caso 11 con NVS borrada
        for _ in range(3):
            self.b.emu.pieza(None, self.b.t)
        self.b.correr(1)
        self.b.emu.seq = 0                    # el ESP32 "olvido" su numeracion
        self.b.correr(1.5)                    # latido con ultimo_seq 0 < 3
        self.assertEqual(self.b.db.epoca(), 2)
        self.b.emu.pieza(None, self.b.t)
        self.b.correr(0.5)
        self.assertEqual(len(self.b.db.piezas()), 4)   # la nueva pieza seq 1 no es "duplicada"

    def test_comandos_del_panel(self):
        ok, _ = self.b.sup.comando({"cmd": "CAPACIDAD", "categoria": "A", "capacidad": 20})
        self.assertTrue(ok)
        ok, _ = self.b.sup.comando({"cmd": "VACIADO", "categoria": "B"})
        self.assertTrue(ok)
        self.b.correr(0.5)
        self.assertEqual(self.b.db.contenedores()["A"]["capacidad"], 20)
        ok, msg = self.b.sup.comando({"cmd": "CAPACIDAD", "categoria": "A", "capacidad": 0})
        self.assertFalse(ok)
        ok, msg = self.b.sup.comando({"cmd": "NADA"})
        self.assertFalse(ok)
        ok, _ = self.b.sup.comando({"cmd": "PARO"})
        self.b.correr(0.5)
        self.assertEqual(self.b.sup.estado_esp32, "EMERGENCIA")

    def test_tramas_invalidas_se_cuentan(self):
        self.b.sup.procesar_linea("basura sin checksum", self.b.t)
        self.b.sup.procesar_linea("PIEZA,1,04AA,A,OK,1,0*00", self.b.t)
        self.assertEqual(self.b.sup.stats["invalidas"], 2)
        self.assertEqual(self.seqs(), [])

    def test_estabilidad_20_piezas(self):   # caso 13
        uids = [None, "04A31FB26C8000", "04FFEEDDCCBB00"]
        self.b.db.registrar_etiqueta("04A31FB26C8000", "x", "B")
        self.b.correr(2.5)
        for i in range(20):
            self.b.emu.pieza(uids[i % 3], self.b.t)
            self.b.correr(0.4)
        self.b.correr(2)
        self.assertEqual(self.seqs(), list(range(1, 21)))
        c = self.b.db.conteos()
        self.assertEqual(c["A"] + c["B"] + c["R"], 20)
        self.assertEqual(self.b.sup.stats["invalidas"], 0)
        emu = self.b.emu.contenido
        self.assertEqual((c["A"], c["B"], c["R"]), (emu["A"], emu["B"], emu["R"]))


if __name__ == "__main__":
    unittest.main()
