const test = require("node:test");
const assert = require("node:assert/strict");
const F = require("../js/formato.js");

test("esc neutraliza HTML, comillas y &", () => {
  assert.equal(F.esc("<img src=x onerror=\"a()\">&'"), "&lt;img src=x onerror=&quot;a()&quot;&gt;&amp;&#39;");
});

test("esc de null y undefined es cadena vacía; de un número, su texto", () => {
  assert.equal(F.esc(null), "");
  assert.equal(F.esc(undefined), "");
  assert.equal(F.esc(5), "5");
});

test("duracion formatea segundos con un decimal y null como raya", () => {
  assert.equal(F.duracion(4630), "4.6 s");
  assert.equal(F.duracion(null), "—");
});

test("uidCorto abrevia el UID y nombra la ausencia", () => {
  assert.equal(F.uidCorto("04A31FB26C8000"), "04A3…00");
  assert.equal(F.uidCorto(null), "Sin etiqueta");
});

test("hora da HH:MM:SS en hora local", () => {
  assert.equal(F.hora(new Date(2026, 10, 24, 9, 5, 7).getTime()), "09:05:07");
});

test("los 10 estados del ESP32 tienen texto y tono permitido", () => {
  const estados = ["PAUSA", "ESPERA", "LECTURA_NFC", "CLASIFICANDO", "LIBERA_INSPECCION",
    "CONFIRMA_SALIDA", "RETORNO", "PAUSA_CONTENEDOR", "ERROR", "EMERGENCIA"];
  const tonos = ["ok", "aviso", "critico", "neutro", "lectura"];
  for (const e of estados) {
    assert.ok(F.ESTADOS[e].texto.length > 0, e);
    assert.ok(tonos.includes(F.ESTADOS[e].tono), e);
  }
  assert.equal(F.ESTADOS.LECTURA_NFC.tono, "lectura");
  assert.equal(F.ESTADOS.EMERGENCIA.tono, "critico");
  assert.equal(F.ESTADOS.PAUSA_CONTENEDOR.tono, "aviso");
});

test("textoEstado cubre null y estados desconocidos", () => {
  assert.equal(F.textoEstado(null), "Sin datos");
  assert.equal(F.textoEstado("RARO"), "RARO");
  assert.equal(F.textoEstado("ESPERA"), "Esperando pieza");
});

test("etiquetaMotivo traduce los motivos de rechazo", () => {
  assert.equal(F.etiquetaMotivo("SIN_LECTURA"), "Sin lectura de etiqueta");
  assert.equal(F.etiquetaMotivo("NO_REGISTRADA"), "Etiqueta no registrada");
  assert.equal(F.etiquetaMotivo(null), "");
});
