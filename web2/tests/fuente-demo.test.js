const test = require("node:test");
const assert = require("node:assert/strict");
require("../js/formato.js");
const Modelo = require("../js/modelo.js");
const FuenteReal = require("../js/fuente-real.js");
const FuenteDemo = require("../js/fuente-demo.js");

function nuevo(valorAzar = 0.4) {
  const t = { v: 1_000_000, r: valorAzar };
  const m = Modelo.crear();
  const d = FuenteDemo.crear(m, { azar: () => t.r, ahora: () => t.v, setTimeout: () => 0, clearTimeout: () => {} });
  d.iniciar();
  return { m, d, t };
}
const pasos = (d, n) => { for (let i = 0; i < n; i++) d.avanzar(); };

test("iniciar marca modo demo y arranca esperando pieza con 3 etiquetas inventadas", () => {
  const { m } = nuevo();
  assert.equal(m.modo, "demo");
  assert.equal(m.estado, "ESPERA");
  assert.equal(m.etiquetas.length, 3);
  assert.equal(m.contenedores.A.capacidad, 15);
  assert.equal(m.enlaceOk, true);
});

test("una pieza normal recorre los estados en orden y emite una PIEZA con seq 1", () => {
  const { m, d } = nuevo();
  const vistos = [];
  for (let i = 0; i < 6; i++) { d.avanzar(); vistos.push(m.estado); }
  assert.deepEqual(vistos, ["LECTURA_NFC", "CLASIFICANDO", "LIBERA_INSPECCION", "CONFIRMA_SALIDA", "RETORNO", "ESPERA"]);
  assert.equal(m.piezas.length, 1);
  assert.equal(m.ultimaPieza().seq, 1);
  assert.equal(m.ultimaPieza().epoca, 1);
  const ciclo = m.ultimaPieza().ciclo_ms;
  assert.ok(ciclo >= 4000 && ciclo <= 5500);
});

test("la categoría sigue la tabla local y el contenedor sube", () => {
  const { m, d } = nuevo(0.4);
  pasos(d, 5);
  assert.equal(m.ultimaPieza().categoria, "B");
  assert.equal(m.ultimaPieza().nombre, "Tornillo");
  assert.equal(m.contenedores.B.contenido, 1);
});

test("inyectar SIN_ETIQUETA manda la siguiente pieza a R con motivo SIN_LECTURA y uid null", () => {
  const { m, d } = nuevo();
  d.inyectar("SIN_ETIQUETA");
  pasos(d, 5);
  const p = m.ultimaPieza();
  assert.equal(p.categoria, "R");
  assert.equal(p.motivo, "SIN_LECTURA");
  assert.equal(p.uid, null);
  assert.equal(m.contenedores.R.contenido, 1);
});

test("NO_REGISTRADA va a R y su UID aparece sin registrar; al registrarlo se clasifica bien", async () => {
  const { m, d, t } = nuevo();
  d.inyectar("NO_REGISTRADA");
  pasos(d, 5);
  const p = m.ultimaPieza();
  assert.equal(p.categoria, "R");
  assert.equal(p.motivo, "NO_REGISTRADA");
  assert.deepEqual(m.uidsSinRegistrar(), [p.uid]);

  await d.comandos.registrarEtiqueta(p.uid, "Cable", "A");
  assert.deepEqual(m.uidsSinRegistrar(), []);
  assert.equal(m.etiquetas.length, 4);

  pasos(d, 1);          // ESPERA normal
  t.r = 0.8;            // elige la 4.ª etiqueta, la recién registrada
  pasos(d, 5);
  assert.equal(m.ultimaPieza().uid, p.uid);
  assert.equal(m.ultimaPieza().categoria, "A");
});

test("ATASCO lleva a ERROR con alerta crítica y no avanza hasta restablecer", () => {
  const { m, d } = nuevo();
  d.inyectar("ATASCO");
  pasos(d, 4);
  assert.equal(m.estado, "ERROR");
  assert.equal(m.alertas[0].codigo, "ATASCO");
  assert.equal(m.alertas[0].nivel, "critico");
  pasos(d, 3);
  assert.equal(m.estado, "ERROR");
  assert.equal(m.piezas.length, 0);
  d.restablecer();
  assert.equal(m.estado, "ESPERA");
});

test("CONTENEDOR_LLENO pausa hasta que se vacía el contenedor", async () => {
  const { m, d } = nuevo(0.4);
  d.inyectar("CONTENEDOR_LLENO");
  pasos(d, 2);
  assert.equal(m.estado, "PAUSA_CONTENEDOR");
  assert.equal(m.alertas[0].nivel, "advertencia");
  pasos(d, 3);
  assert.equal(m.estado, "PAUSA_CONTENEDOR");
  await d.comandos.vaciado("B");
  assert.equal(m.estado, "CLASIFICANDO");
  assert.equal(m.contenedores.B.contenido, 0);
});

test("el paro lleva a EMERGENCIA con alerta crítica y solo restablecer lo saca", async () => {
  const { m, d } = nuevo();
  await d.comandos.paro();
  assert.equal(m.estado, "EMERGENCIA");
  assert.equal(m.alertas[0].codigo, "PARO_PANEL");
  pasos(d, 2);
  assert.equal(m.estado, "EMERGENCIA");
  d.restablecer();
  assert.equal(m.estado, "ESPERA");
});

test("ENLACE_CAIDO apaga el enlace y vuelve solo a los 8 s", () => {
  const { m, d, t } = nuevo();
  d.inyectar("ENLACE_CAIDO");
  assert.equal(m.enlaceOk, false);
  t.v += 3000;
  d.avanzar();
  assert.equal(m.enlaceOk, false);
  t.v += 6000;
  d.avanzar();
  assert.equal(m.enlaceOk, true);
});

test("los comandos del demo tienen los mismos nombres que los reales", () => {
  const { d } = nuevo();
  const reales = Object.keys(FuenteReal.comandos({ fetch: async () => ({}) })).sort();
  assert.deepEqual(Object.keys(d.comandos).sort(), reales);
});

test("registrarEtiqueta valida UID, nombre y categoría con mensajes claros", async () => {
  const { d } = nuevo();
  await assert.rejects(d.comandos.registrarEtiqueta("xyz", "Algo", "A"), /UID inválido/);
  await assert.rejects(d.comandos.registrarEtiqueta("04:A3:1F:B2", "", "A"), /nombre es obligatorio/);
  await assert.rejects(d.comandos.registrarEtiqueta("04:A3:1F:B2", "Algo", "Z"), /categoría/);
  const r = await d.comandos.registrarEtiqueta("04:a3:1f:b2", "Algo", "B");
  assert.equal(r.ok, true);
});

test("capacidad exige un número entre 1 y 999; eliminar exige una etiqueta existente", async () => {
  const { m, d } = nuevo();
  await assert.rejects(d.comandos.capacidad("A", 0), /entre 1 y 999/);
  await assert.rejects(d.comandos.capacidad("A", 1000), /entre 1 y 999/);
  await d.comandos.capacidad("A", 20);
  assert.equal(m.contenedores.A.capacidad, 20);
  await assert.rejects(d.comandos.eliminarEtiqueta("FFFF"), /no existe/);
  await d.comandos.eliminarEtiqueta(m.etiquetas[0].uid);
  assert.equal(m.etiquetas.length, 2);
});
