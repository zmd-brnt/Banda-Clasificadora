const test = require("node:test");
const assert = require("node:assert/strict");
require("../js/formato.js");
const Modelo = require("../js/modelo.js");
require("../js/etapas.js");
require("../js/graficas.js");
const Vistas = require("../js/vistas.js");

const AHORA = 1_000_000;

test("pestanas en supervisión: 4 secciones y sin Etiquetas", () => {
  const html = Vistas.pestanas("supervision");
  assert.equal((html.match(/data-vista=/g) || []).length, 4);
  assert.ok(!/etiquetas/i.test(html));
  assert.ok(!html.includes("data-accion"));
});

test("pestanas en operación: 5 secciones, con Etiquetas", () => {
  const html = Vistas.pestanas("operacion");
  assert.equal((html.match(/data-vista=/g) || []).length, 5);
  assert.ok(html.includes('data-vista="etiquetas"'));
});

test("pestanas marca la activa con aria-current y muestra las alertas pendientes", () => {
  const html = Vistas.pestanas("operacion", { activa: "alertas", alertas: 3 });
  assert.match(html, /data-vista="alertas"[^>]*aria-current="page"/);
  assert.equal((html.match(/aria-current="page"/g) || []).length, 1);
  assert.ok(!html.includes("role=\"tab\""), "sin roles de pestaña incompletos");
  assert.match(html, /3 sin reconocer/);
});

test("encabezado en supervisión no contiene ninguna acción", () => {
  const m = Modelo.crear();
  m.aplicar({ tipo: "ESTADO", estado: "ESPERA" });
  const html = Vistas.encabezado(m, { modo: "supervision", ahora: AHORA });
  assert.ok(!html.includes("data-accion"));
  assert.ok(!html.includes("PARO"));
});

test("encabezado en operación trae el paro y su leyenda", () => {
  const m = Modelo.crear();
  const html = Vistas.encabezado(m, { modo: "operacion", ahora: AHORA });
  assert.match(html, /data-accion="paro"/);
  assert.ok(html.includes("Paro por software; el seguro es el botón físico"));
});

test("encabezado muestra enlace, estado y nombre", () => {
  const m = Modelo.crear();
  m.aplicar({ tipo: "LATIDO", ts: 1, pendientes: 0, version_tabla: 1 });
  m.aplicar({ tipo: "ESTADO", estado: "LECTURA_NFC" });
  const html = Vistas.encabezado(m, { modo: "operacion", ahora: AHORA });
  assert.ok(html.includes("Enlace con el ESP32: conectado"));
  assert.ok(html.includes("Leyendo etiqueta"));
  assert.ok(html.includes("Banda Clasificadora"));
  m.aplicar({ tipo: "ENLACE", ok: false });
  assert.ok(Vistas.encabezado(m, { modo: "operacion", ahora: AHORA }).includes("Enlace con el ESP32: sin latido"));
});

test("encabezado avisa cuando el navegador pierde la conexión con la Pi", () => {
  const m = Modelo.crear();
  m.aplicar({ tipo: "SERVIDOR", ok: false });
  const html = Vistas.encabezado(m, { modo: "supervision", ahora: AHORA });
  assert.match(html, /role="alert"[^>]*>[^<]*Sin conexión con la Pi; reintentando/);
});

test("encabezado muestra la franja de demostración solo en demo", () => {
  const m = Modelo.crear();
  assert.ok(!Vistas.encabezado(m, { modo: "operacion", ahora: AHORA }).includes("Modo demostración"));
  m.modo = "demo";
  assert.ok(Vistas.encabezado(m, { modo: "operacion", ahora: AHORA }).includes("Modo demostración — datos inventados"));
});

test("encabezado con modelo vacío o estado desconocido no muestra undefined ni null", () => {
  const m = Modelo.crear();
  for (const modo of ["operacion", "supervision"]) {
    const html = Vistas.encabezado(m, { modo, ahora: AHORA });
    assert.ok(!/undefined|null|NaN/.test(html), modo);
    assert.ok(html.includes("Sin datos"));
  }
  m.aplicar({ tipo: "ESTADO", estado: "RARO<x>" });
  assert.ok(Vistas.encabezado(m, { modo: "operacion", ahora: AHORA }).includes("RARO&lt;x&gt;"));
});

test("el logo se dibuja con currentColor y tiene nombre accesible", () => {
  for (const v of ["simbolo", "horizontal"]) {
    const svg = Vistas.logo(v);
    assert.match(svg, /role="img"/);
    assert.match(svg, /aria-label="Triar"/);
    assert.ok(svg.includes("currentColor"));
    assert.ok(!svg.includes("#000"));
  }
});

test("mientras no hay fuente de datos el encabezado dice que está conectando", () => {
  const m = Modelo.crear();
  assert.match(Vistas.encabezado(m, { modo: "supervision", ahora: AHORA }), /role="status"[^>]*>[^<]*Conectando con la Pi/);
  m.modo = "real";
  assert.ok(!Vistas.encabezado(m, { modo: "supervision", ahora: AHORA }).includes("Conectando con la Pi"));
  m.modo = "demo";
  assert.ok(!Vistas.encabezado(m, { modo: "supervision", ahora: AHORA }).includes("Conectando con la Pi"));
});
