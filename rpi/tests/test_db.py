import os
import tempfile
import unittest

from banda import protocolo
from banda.db import BaseDatos


def pieza(seq, uid="04AA", cat="A", motivo=None, reenvio=False):
    return {"seq": seq, "uid": uid, "categoria": cat, "motivo": motivo, "ciclo_ms": 3000, "reenvio": reenvio}


class TestBaseDatos(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.ruta = os.path.join(self.dir.name, "prueba.db")
        self.db = BaseDatos(self.ruta)

    def tearDown(self):
        self.db.cerrar()
        self.dir.cleanup()

    def test_valores_iniciales(self):
        self.assertEqual(self.db.version_tabla(), 1)
        self.assertEqual(self.db.epoca(), 1)
        self.assertEqual(self.db.contenedores()["A"], {"contenido": 0, "capacidad": 15})
        self.assertEqual(self.db.etiquetas(), [])

    def test_etiquetas_y_version(self):
        v = self.db.registrar_etiqueta("04:aa:bb", "Tornillo", "A")
        self.assertEqual(v, 2)
        self.assertEqual(self.db.tabla_sync(), {"04AABB": "A"})
        self.assertEqual(self.db.nombre_de("04AABB"), "Tornillo")
        v = self.db.registrar_etiqueta("04AABB", "Tornillo M3", "B")   # actualizar
        self.assertEqual(v, 3)
        self.assertEqual(self.db.etiquetas()[0]["categoria"], "B")
        self.assertEqual(self.db.eliminar_etiqueta("04AABB"), 4)
        self.assertIsNone(self.db.eliminar_etiqueta("04AABB"))
        self.assertEqual(self.db.version_tabla(), 4)

    def test_etiqueta_invalida_no_cambia_version(self):
        for args in [("zz", "x", "A"), ("04AA", "", "A"), ("04AA", "x", "C")]:
            with self.subTest(args=args), self.assertRaises(protocolo.ErrorProtocolo):
                self.db.registrar_etiqueta(*args)
        self.assertEqual(self.db.version_tabla(), 1)

    def test_limite_de_etiquetas(self):
        for i in range(protocolo.MAX_ETIQUETAS):
            self.db.registrar_etiqueta(f"04{i:012X}", f"obj {i}", "A")
        with self.assertRaises(protocolo.ErrorProtocolo):
            self.db.registrar_etiqueta("04FFFFFFFFFFFF", "uno de mas", "B")
        self.assertEqual(len(self.db.etiquetas()), protocolo.MAX_ETIQUETAS)

    def test_piezas_sin_duplicados_y_conteos(self):
        self.assertTrue(self.db.guardar_pieza(1, pieza(1), 1000, False))
        self.assertFalse(self.db.guardar_pieza(1, pieza(1), 1000, False))   # duplicada
        self.db.guardar_pieza(1, pieza(2, uid=None, cat="R", motivo="SIN_LECTURA"), 1000, False)
        self.db.guardar_pieza(1, pieza(4, uid="04BB", cat="R", motivo="NO_REGISTRADA"), 1000, False)
        self.assertTrue(self.db.existe_pieza(1, 1))
        self.assertEqual(self.db.max_seq(1), 4)
        self.assertEqual(self.db.faltantes(1, 4), [3])
        self.assertEqual(self.db.conteos(), {"A": 1, "B": 0, "R": 2, "SIN_LECTURA": 1, "NO_REGISTRADA": 1})
        self.assertEqual([p["seq"] for p in self.db.piezas()], [1, 2, 4])

    def test_epocas(self):
        self.db.guardar_pieza(1, pieza(1), 1000, False)
        self.assertEqual(self.db.nueva_epoca(), 2)
        self.assertTrue(self.db.guardar_pieza(2, pieza(1), 2000, False))   # mismo seq, otra epoca
        self.assertEqual(self.db.max_seq(2), 1)
        self.assertEqual(len(self.db.piezas()), 2)

    def test_persistencia(self):
        self.db.registrar_etiqueta("04AA", "x", "A")
        self.db.guardar_pieza(1, pieza(1), 1000, False)
        self.db.cerrar()
        self.db = BaseDatos(self.ruta)
        self.assertEqual(self.db.version_tabla(), 2)
        self.assertEqual(len(self.db.piezas()), 1)

    def test_alertas_y_contenedores(self):
        a = self.db.guardar_alerta("critico", "ATASCO", "texto")
        self.assertFalse(a["reconocida"])
        self.db.reconocer_alertas()
        self.assertTrue(self.db.alertas()[0]["reconocida"])
        self.db.actualizar_contenedores({"A": 3, "B": 0, "R": 1}, {"A": 15, "B": 15, "R": 20})
        self.assertEqual(self.db.contenedores()["R"], {"contenido": 1, "capacidad": 20})


if __name__ == "__main__":
    unittest.main()
