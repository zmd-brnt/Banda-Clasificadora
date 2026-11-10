import unittest

from banda import protocolo as p


class TestTramas(unittest.TestCase):
    def test_checksum_y_trama(self):
        self.assertEqual(p.checksum("ACK,1"), 0x41 ^ 0x43 ^ 0x4B ^ 0x2C ^ 0x31)
        self.assertEqual(p.trama("ACK,1"), f"ACK,1*{p.checksum('ACK,1'):02X}\n".encode())

    def test_validar(self):
        t = p.trama("LATIDO,0,3,7").decode()
        self.assertEqual(p.validar(t), "LATIDO,0,3,7")
        self.assertEqual(p.validar(t.strip() + "\r\n"), "LATIDO,0,3,7")
        self.assertIsNone(p.validar("LATIDO,0,3,7*00"))
        self.assertIsNone(p.validar("LATIDO,0,3,7"))
        self.assertIsNone(p.validar("*00"))
        self.assertIsNone(p.validar("ACK,1*ZZ"))

    def test_trama_demasiado_larga(self):
        with self.assertRaises(p.ErrorProtocolo):
            p.trama("X" * 800)

    def test_mismo_checksum_que_el_firmware(self):
        # Valor de referencia calculado en firmware/test (proto::checksum("ACK,1"))
        self.assertEqual(p.trama("ACK,1"), b"ACK,1*" + f"{0x41 ^ 0x43 ^ 0x4B ^ 0x2C ^ 0x31:02X}".encode() + b"\n")


class TestInterpretar(unittest.TestCase):
    def test_pieza(self):
        m = p.interpretar("PIEZA,12,04A1B2C3D4E5F6,A,OK,3200,0")
        self.assertEqual(m, {"tipo": "PIEZA", "seq": 12, "uid": "04A1B2C3D4E5F6", "categoria": "A",
                             "motivo": None, "ciclo_ms": 3200, "reenvio": False})
        m = p.interpretar("PIEZA,13,-,R,SIN_LECTURA,1500,1")
        self.assertIsNone(m["uid"])
        self.assertEqual(m["motivo"], "SIN_LECTURA")
        self.assertTrue(m["reenvio"])

    def test_pieza_invalida(self):
        for c in ["PIEZA,1,04AA,C,OK,1,0", "PIEZA,1,04AA,A,RARO,1,0", "PIEZA,x,04AA,A,OK,1,0",
                  "PIEZA,1,04AA,A,OK,1", "PIEZA,1,04AA,A,OK,1,2", "PIEZA,1,04ZZ,A,OK,1,0"]:
            with self.subTest(c=c), self.assertRaises(p.ErrorProtocolo):
                p.interpretar(c)

    def test_latido_3_y_4_campos(self):
        self.assertEqual(p.interpretar("LATIDO,2,5")["ultimo_seq"], None)   # integracion_test
        m = p.interpretar("LATIDO,2,5,40")
        self.assertEqual((m["pendientes"], m["version_tabla"], m["ultimo_seq"]), (2, 5, 40))

    def test_alerta_estado_contenedores(self):
        self.assertEqual(p.interpretar("ALERTA,ATASCO,SALIDA"),
                         {"tipo": "ALERTA", "codigo": "ATASCO", "detalle": "SALIDA"})
        self.assertEqual(p.interpretar("ALERTA,REINICIO")["detalle"], None)
        self.assertEqual(p.interpretar("ESTADO,LECTURA_NFC")["estado"], "LECTURA_NFC")
        m = p.interpretar("CONTENEDORES,1,2,3,15,15,20")
        self.assertEqual(m["contenido"], {"A": 1, "B": 2, "R": 3})
        self.assertEqual(m["capacidad"], {"A": 15, "B": 15, "R": 20})
        with self.assertRaises(p.ErrorProtocolo):
            p.interpretar("ESTADO,minusculas")
        with self.assertRaises(p.ErrorProtocolo):
            p.interpretar("CONTENEDORES,1,2,3")
        with self.assertRaises(p.ErrorProtocolo):
            p.interpretar("DESCONOCIDO,1")


class TestComandos(unittest.TestCase):
    def test_sync_ordenado_y_normalizado(self):
        c = p.cmd_sync(4, {"04:bb": "B", "04aa": "A"})
        self.assertEqual(c, "SYNC,4,04AA=A;04BB=B")
        self.assertEqual(p.cmd_sync(5, {}), "SYNC,5,")

    def test_sync_limites(self):
        demasiadas = {f"04{i:012X}": "A" for i in range(p.MAX_ETIQUETAS + 1)}
        with self.assertRaises(p.ErrorProtocolo):
            p.cmd_sync(1, demasiadas)
        with self.assertRaises(p.ErrorProtocolo):
            p.cmd_sync(1, {"04AA": "C"})
        # 32 UID de 20 caracteres no caben en 700 caracteres
        largas = {f"{i:020X}": "A" for i in range(p.MAX_ETIQUETAS)}
        with self.assertRaises(p.ErrorProtocolo):
            p.cmd_sync(1, largas)
        # 32 UID de 14 caracteres (NTAG213) si caben
        ntag = {f"04{i:012X}": "A" for i in range(p.MAX_ETIQUETAS)}
        p.trama(p.cmd_sync(1, ntag))

    def test_otros(self):
        self.assertEqual(p.cmd_ack(3), "ACK,3")
        self.assertEqual(p.cmd_vivo(2), "VIVO,2")
        self.assertEqual(p.cmd_reenviar(9), "REENVIAR,9")
        self.assertEqual(p.cmd_vaciado("B"), "VACIADO,B")
        self.assertEqual(p.cmd_capacidad("R", "20"), "CAPACIDAD,R,20")
        self.assertEqual(p.cmd_paro(), "PARO")
        for args in [("X", 5), ("A", 0), ("A", 1000)]:
            with self.subTest(args=args), self.assertRaises(p.ErrorProtocolo):
                p.cmd_capacidad(*args)

    def test_normalizar_uid(self):
        self.assertEqual(p.normalizar_uid("04:a1:b2 c3"), "04A1B2C3")
        for malo in ["", "04:GG", "0" * 21]:
            with self.subTest(malo=malo), self.assertRaises(p.ErrorProtocolo):
                p.normalizar_uid(malo)

    def test_describir_alerta(self):
        self.assertEqual(p.describir_alerta("REINICIO", "BROWNOUT")[0], "critico")
        nivel, msg = p.describir_alerta("CONTENEDOR_LLENO", "A")
        self.assertEqual(nivel, "advertencia")
        self.assertIn("A", msg)
        self.assertEqual(p.describir_alerta("NUEVO_CODIGO", None)[0], "advertencia")


if __name__ == "__main__":
    unittest.main()
