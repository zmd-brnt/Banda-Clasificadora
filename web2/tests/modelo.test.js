const test = require("node:test");
const assert = require("node:assert/strict");
require("../js/formato.js");
const Modelo = require("../js/modelo.js");

const pieza = (seq, extra = {}) => ({
  tipo: "PIEZA", epoca: 1, seq, ts: 1_000_000 + seq * 1000, uid: "04A31FB26C8000", categoria: "A",
  motivo: null, ciclo_ms: 4600, reenvio: false, tardio: false, nombre: "Llave", ...extra,
});
const instantanea = (extra = {}) => ({
  enlace_ok: true, estado: "ESPERA", version_tabla: 2, version_esp32: 2, pendientes: 0,
  etiquetas: [{ uid: "04A31FB26C8000", nombre: "Llave", categoria: "A" }],
  piezas: [pieza(1), pieza(2, { categoria: "B" }), pieza(3, { categoria: "R", motivo: "SIN_LECTURA", uid: null })],
  alertas: [], contenedores: { A: { contenido: 1, capacidad: 15 }, B: { contenido: 1, capacidad: 15 }, R: { contenido: 1, capacidad: 15 } },
  stats: {}, ...extra,
});

test("la instantánea carga estado, piezas y contenedores", () => {
  const m = Modelo.crear();
  m.aplicarInstantanea(instantanea());
  assert.equal(m.conteos().total, 3);
  assert.equal(m.estado, "ESPERA");
  assert.equal(m.contenedores.A.capacidad, 15);
  assert.equal(m.enlaceOk, true);
});

test("una PIEZA nueva se agrega, avisa una sola vez y es la última", () => {
  const m = Modelo.crear();
  m.aplicarInstantanea(instantanea());
  let avisos = 0;
  m.suscribir(() => avisos++);
  m.aplicar(pieza(4));
  assert.equal(avisos, 1);
  assert.equal(m.ultimaPieza().seq, 4);
});

test("una PIEZA repetida (misma epoca y seq) no se cuenta dos veces", () => {
  const m = Modelo.crear();
  m.aplicarInstantanea(instantanea());
  m.aplicar(pieza(3, { categoria: "R", motivo: "SIN_LECTURA", uid: null }));
  m.aplicar(pieza(4));
  m.aplicar(pieza(4));
  assert.equal(m.piezas.length, 4);
  assert.equal(m.conteos().total, 4);
});

test("la desuscripción deja de avisar", () => {
  const m = Modelo.crear();
  let avisos = 0;
  const baja = m.suscribir(() => avisos++);
  m.aplicar({ tipo: "ESTADO", estado: "ESPERA" });
  baja();
  m.aplicar({ tipo: "ESTADO", estado: "ERROR" });
  assert.equal(avisos, 1);
});

test("LATIDO enlaza y guarda pendientes; ENLACE y SERVIDOR los apagan", () => {
  const m = Modelo.crear();
  m.aplicar({ tipo: "LATIDO", ts: 5, pendientes: 2, version_tabla: 3 });
  assert.equal(m.enlaceOk, true);
  assert.equal(m.pendientes, 2);
  assert.equal(m.versionEsp32, 3);
  m.aplicar({ tipo: "ENLACE", ok: false });
  assert.equal(m.enlaceOk, false);
  m.aplicar({ tipo: "SERVIDOR", ok: false });
  assert.equal(m.servidorOk, false);
});

test("ALERTA entra primero y alertasSinReconocer cuenta las pendientes", () => {
  const m = Modelo.crear();
  m.aplicarInstantanea(instantanea({ alertas: [{ id: 1, ts: 1, nivel: "info", codigo: "X", mensaje: "viejo", reconocida: true }] }));
  m.aplicar({ tipo: "ALERTA", id: 2, ts: 2, nivel: "critico", codigo: "ATASCO", mensaje: "nuevo" });
  assert.equal(m.alertas[0].codigo, "ATASCO");
  assert.equal(m.alertasSinReconocer(), 1);
});

test("CONTENEDORES y ETIQUETAS reemplazan los datos", () => {
  const m = Modelo.crear();
  m.aplicar({ tipo: "CONTENEDORES", contenedores: { A: { contenido: 9, capacidad: 20 }, B: { contenido: 0, capacidad: 20 }, R: { contenido: 0, capacidad: 20 } } });
  m.aplicar({ tipo: "ETIQUETAS", version: 7, etiquetas: [{ uid: "AA", nombre: "Tornillo", categoria: "B" }] });
  assert.equal(m.contenedores.A.contenido, 9);
  assert.equal(m.versionTabla, 7);
  assert.equal(m.etiquetas.length, 1);
});

test("el feed guarda máximo 50 y nunca incluye LATIDO", () => {
  const m = Modelo.crear();
  for (let i = 0; i < 60; i++) m.aplicar({ tipo: "ESTADO", estado: i % 2 ? "ESPERA" : "LECTURA_NFC", ts: i });
  for (let i = 0; i < 5; i++) m.aplicar({ tipo: "LATIDO", ts: i, pendientes: 0, version_tabla: 1 });
  assert.equal(m.feed.length, 50);
  assert.ok(m.feed.every((e) => e.tipo !== "LATIDO"));
});

test("el feed describe piezas y rechazos con texto legible", () => {
  const m = Modelo.crear();
  m.aplicar(pieza(1));
  m.aplicar(pieza(2, { categoria: "R", motivo: "NO_REGISTRADA" }));
  assert.match(m.feed[1].texto, /Pieza #1 → A/);
  assert.match(m.feed[0].texto, /rechazada: Etiqueta no registrada/);
});

test("conteos separa categorías y motivos", () => {
  const m = Modelo.crear();
  m.aplicarInstantanea(instantanea());
  assert.deepEqual(m.conteos(), { total: 3, A: 1, B: 1, R: 1, SIN_LECTURA: 1, NO_REGISTRADA: 0 });
});

test("piezasPorMinuto cuenta por minuto, del más antiguo al más reciente", () => {
  const m = Modelo.crear();
  const ahora = 10_000_000;
  m.aplicar(pieza(1, { ts: ahora - 10_000 }));
  m.aplicar(pieza(2, { ts: ahora - 30_000 }));
  m.aplicar(pieza(3, { ts: ahora - 3 * 60_000 - 5_000 }));
  const c = m.piezasPorMinuto(ahora, 10);
  assert.equal(c.length, 10);
  assert.equal(c[9], 2);
  assert.equal(c[6], 1);
  assert.equal(c.reduce((a, b) => a + b, 0), 3);
});

test("ciclosRecientes ignora null y 0", () => {
  const m = Modelo.crear();
  m.aplicar(pieza(1, { ciclo_ms: 4000 }));
  m.aplicar(pieza(2, { ciclo_ms: null }));
  m.aplicar(pieza(3, { ciclo_ms: 0 }));
  m.aplicar(pieza(4, { ciclo_ms: 5000 }));
  assert.deepEqual(m.ciclosRecientes(), [4000, 5000]);
});

test("uidsSinRegistrar excluye los ya registrados y no repite", () => {
  const m = Modelo.crear();
  m.aplicar({ tipo: "ETIQUETAS", version: 1, etiquetas: [{ uid: "AA", nombre: "x", categoria: "A" }] });
  m.aplicar(pieza(1, { uid: "AA", categoria: "R", motivo: "NO_REGISTRADA" }));
  m.aplicar(pieza(2, { uid: "BB", categoria: "R", motivo: "NO_REGISTRADA" }));
  m.aplicar(pieza(3, { uid: "BB", categoria: "R", motivo: "NO_REGISTRADA" }));
  assert.deepEqual(m.uidsSinRegistrar(), ["BB"]);
});

test("un modelo vacío no falla ni produce valores raros", () => {
  const m = Modelo.crear();
  assert.deepEqual(m.conteos(), { total: 0, A: 0, B: 0, R: 0, SIN_LECTURA: 0, NO_REGISTRADA: 0 });
  assert.equal(m.ultimaPieza(), null);
  assert.deepEqual(m.piezasPorMinuto(Date.now()), [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
  assert.deepEqual(m.ciclosRecientes(), []);
  assert.equal(m.porcentajeContenedor("A"), 0);
  assert.equal(m.estado, null);
});

test("porcentajeContenedor evita dividir entre cero y limita a 0–100", () => {
  const m = Modelo.crear();
  m.aplicar({ tipo: "CONTENEDORES", contenedores: { A: { contenido: 7, capacidad: 15 }, B: { contenido: 3, capacidad: 0 }, R: { contenido: 20, capacidad: 15 } } });
  assert.equal(m.porcentajeContenedor("A"), 47);
  assert.equal(m.porcentajeContenedor("B"), 0);
  assert.equal(m.porcentajeContenedor("R"), 100);
});

test("marcarAlertasReconocidas deja todas reconocidas y avisa", () => {
  const m = Modelo.crear();
  m.aplicar({ tipo: "ALERTA", id: 1, ts: 1, nivel: "critico", codigo: "A", mensaje: "x" });
  m.aplicar({ tipo: "ALERTA", id: 2, ts: 2, nivel: "info", codigo: "B", mensaje: "y" });
  let avisos = 0;
  m.suscribir(() => avisos++);
  m.marcarAlertasReconocidas();
  assert.equal(m.alertasSinReconocer(), 0);
  assert.equal(avisos, 1);
});
