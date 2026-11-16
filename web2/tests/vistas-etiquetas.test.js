const test = require("node:test");
const assert = require("node:assert/strict");
require("../js/formato.js");
const Modelo = require("../js/modelo.js");
require("../js/etapas.js");
require("../js/graficas.js");
const Vistas = require("../js/vistas.js");

const AHORA = 10_000_000;
const OP = { modo: "operacion", ahora: AHORA };
const conEtiquetas = (lista = [{ uid: "04A31FB26C8000", nombre: "Llave", categoria: "A" }], version = 2) => {
  const m = Modelo.crear();
  m.aplicar({ tipo: "ETIQUETAS", version, etiquetas: lista });
  return m;
};

test("en supervisión la vista de etiquetas no existe", () => {
  assert.equal(Vistas.etiquetas(conEtiquetas(), { modo: "supervision", ahora: AHORA }), "");
});

test("en operación hay formulario con UID, nombre y categoría", () => {
  const html = Vistas.etiquetas(conEtiquetas(), OP);
  assert.match(html, /<form[^>]*data-accion="registrar-etiqueta"/);
  assert.ok(html.includes('id="campo-uid"'));
  assert.ok(html.includes('id="campo-nombre"'));
  assert.match(html, /<select[^>]*name="categoria"/);
  for (const v of ["A", "B", "R"]) assert.ok(html.includes(`<option value="${v}"`), v);
  assert.ok(html.includes("Registrar etiqueta"));
});

test("siempre ofrece Sincronizar con ESP32", () => {
  assert.match(Vistas.etiquetas(conEtiquetas(), OP), /data-accion="sincronizar"[^>]*>Sincronizar con ESP32</);
});

test("el aviso de versión aparece solo si el ESP32 tiene otra versión de la tabla", () => {
  const m = conEtiquetas([], 3);
  m.aplicar({ tipo: "LATIDO", ts: 1, pendientes: 0, version_tabla: 2 });
  const html = Vistas.etiquetas(m, OP);
  assert.ok(html.includes("desactualizada"));
  assert.ok(html.includes("v2 de v3"));
  m.aplicar({ tipo: "LATIDO", ts: 2, pendientes: 0, version_tabla: 3 });
  assert.ok(!Vistas.etiquetas(m, OP).includes("desactualizada"));
});

test("los UID detectados sin registrar tienen botón de registro rápido", () => {
  const m = conEtiquetas();
  m.aplicar({ tipo: "PIEZA", epoca: 1, seq: 1, ts: AHORA, uid: "04B7A9C8D10000", categoria: "R", motivo: "NO_REGISTRADA", ciclo_ms: 4000, nombre: null });
  const html = Vistas.etiquetas(m, OP);
  assert.match(html, /data-rellenar-uid="04B7A9C8D10000"/);
  assert.ok(!Vistas.etiquetas(conEtiquetas(), OP).includes("data-rellenar-uid"));
});

test("cada etiqueta tiene su botón de eliminar con el UID", () => {
  const html = Vistas.etiquetas(conEtiquetas(), OP);
  assert.match(html, /data-accion="eliminar-etiqueta"[^>]*data-uid="04A31FB26C8000"/);
  assert.ok(html.includes("Llave"));
});

test("nombres con HTML o comillas no escapan del texto ni de los atributos", () => {
  const nombre = '<img src=x onerror=alert(1)> x" onmouseover="alert(2)';
  const html = Vistas.etiquetas(conEtiquetas([{ uid: "AA11", nombre, categoria: "B" }]), OP);
  assert.ok(!html.includes("<img"));
  assert.ok(!html.includes('" onmouseover="'));
  assert.ok(html.includes("&lt;img"));
});

test("sin etiquetas muestra un vacío que invita a registrar la primera", () => {
  const html = Vistas.etiquetas(Modelo.crear(), OP);
  assert.ok(html.includes("Aún no hay etiquetas. Registra la primera arriba."));
  assert.ok(!/NaN|undefined|null/.test(html));
});
