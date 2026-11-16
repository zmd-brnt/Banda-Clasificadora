const test = require("node:test");
const assert = require("node:assert/strict");
require("../js/formato.js");
const Etapas = require("../js/etapas.js");

test("el orden de la banda incluye el sensor de salida antes de la compuerta", () => {
  assert.deepEqual(Etapas.ORDEN, ["entrada", "inspeccion", "lector", "salida", "compuerta"]);
});

test("cada estado del ESP32 ilumina su etapa", () => {
  const esperado = {
    ESPERA: "entrada", LIBERA_INSPECCION: "inspeccion", LECTURA_NFC: "lector",
    CONFIRMA_SALIDA: "salida", CLASIFICANDO: "compuerta", RETORNO: "compuerta",
  };
  for (const [estado, etapa] of Object.entries(esperado)) {
    assert.equal(Etapas.de(estado).etapa, etapa, estado);
  }
});

test("la lectura NFC tiene tono propio", () => {
  assert.equal(Etapas.de("LECTURA_NFC").tono, "lectura");
});

test("pausa, contenedor lleno, error, emergencia y sin datos no iluminan ninguna etapa", () => {
  for (const e of ["PAUSA", "PAUSA_CONTENEDOR", "ERROR", "EMERGENCIA", null]) {
    assert.equal(Etapas.de(e).etapa, null, String(e));
  }
  assert.equal(Etapas.de("EMERGENCIA").tono, "critico");
  assert.equal(Etapas.de("PAUSA_CONTENEDOR").tono, "aviso");
  assert.equal(Etapas.de(null).tono, "neutro");
});
