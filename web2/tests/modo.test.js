const test = require("node:test");
const assert = require("node:assert/strict");
const M = require("../js/modo.js");

test("deAncho: 900 px o más es operación, menos es supervisión", () => {
  assert.equal(M.deAncho(899), "supervision");
  assert.equal(M.deAncho(900), "operacion");
  assert.equal(M.deAncho(390), "supervision");
});

test("en supervisión no se puede ninguna acción", () => {
  for (const a of M.ACCIONES) assert.equal(M.puede("supervision", a), false, a);
});

test("en operación solo se pueden las acciones conocidas", () => {
  assert.equal(M.puede("operacion", "paro"), true);
  assert.equal(M.puede("operacion", "borrar-todo"), false);
});
