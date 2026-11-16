const test = require("node:test");
const assert = require("node:assert/strict");
require("../js/formato.js");
const Modelo = require("../js/modelo.js");
require("../js/etapas.js");
require("../js/graficas.js");
const Vistas = require("../js/vistas.js");

const AHORA = 10_000_000;
const XSS = "<img src=x onerror=alert(1)>";
const pieza = (seq, extra = {}) => ({
  tipo: "PIEZA", epoca: 1, seq, ts: AHORA - 60_000 + seq * 1000, uid: "04A31FB26C8000", categoria: "A",
  motivo: null, ciclo_ms: 4000, reenvio: false, tardio: false, nombre: "Llave", ...extra,
});
const cuenta = (html, re) => (html.match(re) || []).length;

function lleno() {
  const m = Modelo.crear();
  m.aplicar({ tipo: "CONTENEDORES", contenedores: { A: { contenido: 7, capacidad: 15 }, B: { contenido: 2, capacidad: 15 }, R: { contenido: 1, capacidad: 15 } } });
  m.aplicar(pieza(1));
  m.aplicar(pieza(2, { categoria: "B", uid: "04B2C4D6E80000", nombre: "Tornillo" }));
  m.aplicar(pieza(3, { categoria: "R", motivo: "SIN_LECTURA", uid: null, nombre: null }));
  m.aplicar({ tipo: "ALERTA", id: 1, ts: AHORA, nivel: "critico", codigo: "ATASCO", mensaje: "Atasco en inspección" });
  return m;
}
const VISTAS = ["inventario", "historial", "alertas"];

test("en supervisión ninguna lista trae acciones", () => {
  const m = lleno();
  for (const v of VISTAS) {
    assert.ok(!Vistas[v](m, { modo: "supervision", ahora: AHORA, filtro: "todas" }).includes("data-accion"), v);
  }
});

test("inventario en operación trae vaciado y capacidad por contenedor", () => {
  const html = Vistas.inventario(lleno(), { modo: "operacion", ahora: AHORA });
  assert.equal(cuenta(html, /data-accion="vaciado"/g), 3);
  assert.equal(cuenta(html, /data-accion="capacidad"/g), 3);
  const campoA = /<input[^>]*data-capacidad="A"[^>]*>/.exec(html)[0];
  assert.ok(campoA.includes('value="15"'));
  assert.ok(html.includes("Marcar vaciado"));
  assert.ok(html.includes("Cambiar capacidad"));
});

test("inventario muestra ocupación, barra y la tabla por objeto agregada", () => {
  const m = lleno();
  m.aplicar(pieza(4));
  m.aplicar(pieza(5));
  const html = Vistas.inventario(m, { modo: "supervision", ahora: AHORA });
  assert.ok(html.includes("7 de 15"));
  assert.ok(html.includes("--p:47%"));
  assert.equal(cuenta(html, /Llave/g), 1);
  assert.match(html, /Llave[\s\S]*?<td class="num">3</);
  assert.ok(html.includes("Sin etiqueta"));
});

test("historial en operación exporta CSV salvo en demo", () => {
  const real = Vistas.historial(lleno(), { modo: "operacion", ahora: AHORA, filtro: "todas" });
  assert.match(real, /data-accion="exportar-csv"/);
  assert.match(real, /href="api\/historial\.csv"/);
  assert.ok(!Vistas.historial(lleno(), { modo: "operacion", ahora: AHORA, filtro: "todas", demo: true }).includes("exportar-csv"));
});

test("historial filtra por destino y muestra el motivo y el origen", () => {
  const m = lleno();
  m.aplicar(pieza(4, { reenvio: true, tardio: true }));
  const todas = Vistas.historial(m, { modo: "operacion", ahora: AHORA, filtro: "todas" });
  assert.equal(cuenta(todas, /class="fila-pieza"/g), 4);
  assert.ok(todas.includes("Sin lectura de etiqueta"));
  assert.ok(todas.includes("Reenviada"));
  const soloB = Vistas.historial(m, { modo: "operacion", ahora: AHORA, filtro: "B" });
  assert.equal(cuenta(soloB, /class="fila-pieza"/g), 1);
  assert.match(soloB, /<option value="B" selected/);
});

test("historial limita las filas: 200 en operación y 20 en supervisión", () => {
  const m = Modelo.crear();
  for (let i = 1; i <= 250; i++) m.aplicar(pieza(i));
  assert.equal(cuenta(Vistas.historial(m, { modo: "operacion", ahora: AHORA, filtro: "todas" }), /class="fila-pieza"/g), 200);
  assert.equal(cuenta(Vistas.historial(m, { modo: "supervision", ahora: AHORA, filtro: "todas" }), /class="pieza-item"/g), 20);
});

test("historial muestra primero la pieza más reciente", () => {
  const html = Vistas.historial(lleno(), { modo: "operacion", ahora: AHORA, filtro: "todas" });
  assert.ok(html.indexOf("#3") < html.indexOf("#1"));
});

test("alertas muestra el nivel como texto y marca las nuevas", () => {
  const html = Vistas.alertas(lleno(), { modo: "supervision", ahora: AHORA });
  assert.ok(html.includes("Crítica"));
  assert.ok(html.includes("ATASCO"));
  assert.ok(html.includes("Nueva"));
});

test("reconocer todas está desactivado si no hay pendientes", () => {
  const m = lleno();
  assert.ok(!/data-accion="reconocer-alertas"[^>]*disabled/.test(Vistas.alertas(m, { modo: "operacion", ahora: AHORA })));
  m.marcarAlertasReconocidas();
  assert.match(Vistas.alertas(m, { modo: "operacion", ahora: AHORA }), /data-accion="reconocer-alertas"[^>]*disabled/);
});

test("un nombre o mensaje con HTML se muestra como texto en todas las listas", () => {
  const m = Modelo.crear();
  m.aplicar(pieza(1, { nombre: XSS }));
  m.aplicar({ tipo: "ALERTA", id: 1, ts: AHORA, nivel: "info", codigo: "<b>X</b>", mensaje: XSS });
  for (const modo of ["operacion", "supervision"]) {
    for (const v of VISTAS) {
      const html = Vistas[v](m, { modo, ahora: AHORA, filtro: "todas" });
      assert.ok(!html.includes("<img"), `${v} ${modo}`);
      assert.ok(!html.includes("<b>X"), `${v} ${modo}`);
    }
  }
  assert.ok(Vistas.alertas(m, { modo: "operacion", ahora: AHORA }).includes("&lt;img src=x onerror=alert(1)&gt;"));
});

test("sin datos cada lista muestra un vacío claro y ningún valor raro", () => {
  const m = Modelo.crear();
  for (const modo of ["operacion", "supervision"]) {
    const o = { modo, ahora: AHORA, filtro: "todas" };
    assert.ok(Vistas.inventario(m, o).includes("Sin piezas todavía"), modo);
    assert.ok(Vistas.historial(m, o).includes("Sin piezas todavía"), modo);
    assert.ok(Vistas.alertas(m, o).includes("Sin alertas. Todo en orden."), modo);
    for (const v of VISTAS) assert.ok(!/NaN|undefined|null/.test(Vistas[v](m, o)), `${v} ${modo}`);
  }
});

test("contenedores con capacidad 0 o desbordada no rompen el inventario", () => {
  const m = Modelo.crear();
  m.aplicar({ tipo: "CONTENEDORES", contenedores: { A: { contenido: 3, capacidad: 0 }, B: { contenido: 20, capacidad: 15 }, R: { contenido: 0, capacidad: 15 } } });
  const html = Vistas.inventario(m, { modo: "operacion", ahora: AHORA });
  assert.ok(html.includes("--p:0%"));
  assert.ok(html.includes("--p:100%"));
  assert.ok(!/NaN|undefined|null/.test(html));
});
