const test = require("node:test");
const assert = require("node:assert/strict");
require("../js/formato.js");
const Modelo = require("../js/modelo.js");
const FuenteReal = require("../js/fuente-real.js");

const INST = {
  enlace_ok: true, estado: "ESPERA", version_tabla: 1, version_esp32: 1, pendientes: 0, etiquetas: [],
  piezas: [], alertas: [], contenedores: { A: { contenido: 2, capacidad: 15 }, B: { contenido: 0, capacidad: 15 }, R: { contenido: 0, capacidad: 15 } }, stats: {},
};

function fetchFalso(rutas = {}) {
  const llamadas = [];
  const f = async (url, opciones = {}) => {
    const metodo = opciones.method || "GET";
    llamadas.push({ url, metodo, cuerpo: opciones.body ? JSON.parse(opciones.body) : undefined, cabeceras: opciones.headers });
    const r = rutas[`${metodo} ${url}`];
    if (r === "caida") throw new TypeError("fetch failed");
    const { status = 200, body = {} } = r || {};
    return { ok: status >= 200 && status < 300, status, json: async () => body };
  };
  f.llamadas = llamadas;
  return f;
}

class EventSourceFalso {
  constructor(url) { this.url = url; EventSourceFalso.ultimo = this; this.cerrado = false; }
  close() { this.cerrado = true; }
}

test("conectar carga la instantánea y marca modo real", async () => {
  const m = Modelo.crear();
  const f = fetchFalso({ "GET api/estado": { body: INST } });
  await FuenteReal.conectar(m, { fetch: f, EventSource: EventSourceFalso });
  assert.equal(m.modo, "real");
  assert.equal(m.contenedores.A.contenido, 2);
  assert.equal(EventSourceFalso.ultimo.url, "api/eventos");
});

test("conectar rechaza si la Pi no responde o responde con error", async () => {
  const m = Modelo.crear();
  await assert.rejects(FuenteReal.conectar(m, { fetch: fetchFalso({ "GET api/estado": "caida" }), EventSource: EventSourceFalso }));
  await assert.rejects(FuenteReal.conectar(m, { fetch: fetchFalso({ "GET api/estado": { status: 500 } }), EventSource: EventSourceFalso }));
  assert.equal(m.modo, null);
});

test("un mensaje del flujo de eventos llega al modelo", async () => {
  const m = Modelo.crear();
  await FuenteReal.conectar(m, { fetch: fetchFalso({ "GET api/estado": { body: INST } }), EventSource: EventSourceFalso });
  EventSourceFalso.ultimo.onmessage({ data: JSON.stringify({ tipo: "PIEZA", epoca: 1, seq: 1, ts: 1, uid: null, categoria: "R", motivo: "SIN_LECTURA", ciclo_ms: 4000 }) });
  assert.equal(m.conteos().total, 1);
});

test("un mensaje dañado se ignora sin romper el flujo", async () => {
  const m = Modelo.crear();
  await FuenteReal.conectar(m, { fetch: fetchFalso({ "GET api/estado": { body: INST } }), EventSource: EventSourceFalso });
  assert.doesNotThrow(() => EventSourceFalso.ultimo.onmessage({ data: "{no es json" }));
});

test("onerror marca servidorOk falso y al reconectar vuelve a pedir la instantánea", async () => {
  const m = Modelo.crear();
  const f = fetchFalso({ "GET api/estado": { body: INST } });
  await FuenteReal.conectar(m, { fetch: f, EventSource: EventSourceFalso });
  EventSourceFalso.ultimo.onerror();
  assert.equal(m.servidorOk, false);
  await EventSourceFalso.ultimo.onopen();
  assert.equal(m.servidorOk, true);
  assert.equal(f.llamadas.filter((c) => c.url === "api/estado").length, 2);
});

test("cerrar cierra el flujo de eventos", async () => {
  const m = Modelo.crear();
  const c = await FuenteReal.conectar(m, { fetch: fetchFalso({ "GET api/estado": { body: INST } }), EventSource: EventSourceFalso });
  c.cerrar();
  assert.equal(EventSourceFalso.ultimo.cerrado, true);
});

test("los comandos usan las rutas y cuerpos de la API", async () => {
  const f = fetchFalso({
    "POST api/comando": { body: { ok: true, mensaje: "enviado" } },
    "POST api/etiquetas": { body: { ok: true, version: 3 } },
    "DELETE api/etiquetas/04%3AA3": { body: { ok: true, version: 4 } },
    "POST api/alertas/reconocer": { body: { ok: true } },
  });
  const c = FuenteReal.comandos({ fetch: f });
  assert.deepEqual(await c.paro(), { ok: true, mensaje: "enviado" });
  await c.vaciado("A");
  await c.capacidad("B", 20);
  await c.sincronizar();
  assert.equal((await c.registrarEtiqueta("04A31FB26C8000", "Llave", "A")).version, 3);
  await c.eliminarEtiqueta("04:A3");
  await c.reconocerAlertas();
  const cuerpos = f.llamadas.filter((l) => l.url === "api/comando").map((l) => l.cuerpo);
  assert.deepEqual(cuerpos, [{ cmd: "PARO" }, { cmd: "VACIADO", categoria: "A" }, { cmd: "CAPACIDAD", categoria: "B", capacidad: 20 }, { cmd: "SYNC" }]);
  const reg = f.llamadas.find((l) => l.url === "api/etiquetas");
  assert.deepEqual(reg.cuerpo, { uid: "04A31FB26C8000", nombre: "Llave", categoria: "A" });
  assert.equal(reg.cabeceras["Content-Type"], "application/json");
  assert.ok(f.llamadas.some((l) => l.metodo === "DELETE" && l.url === "api/etiquetas/04%3AA3"));
});

test("un error del servidor se lanza con su texto", async () => {
  const f = fetchFalso({
    "POST api/etiquetas": { status: 400, body: { ok: false, error: "UID inválido" } },
    "POST api/comando": { status: 400, body: { ok: false, mensaje: "comando desconocido" } },
  });
  const c = FuenteReal.comandos({ fetch: f });
  await assert.rejects(c.registrarEtiqueta("zz", "x", "A"), { message: "UID inválido" });
  await assert.rejects(c.sincronizar(), { message: "comando desconocido" });
});

test("si la Pi no responde, el comando falla con un mensaje claro", async () => {
  const c = FuenteReal.comandos({ fetch: fetchFalso({ "POST api/comando": "caida" }) });
  await assert.rejects(c.paro(), /conexión con la Pi/i);
});
