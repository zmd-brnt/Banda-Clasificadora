const test = require("node:test");
const assert = require("node:assert/strict");
const G = require("../js/graficas.js");

const O = { ancho: 100, alto: 20 };

test("sparkline vacío o en ceros dibuja una línea plana sin NaN", () => {
  for (const v of [[], [0, 0, 0], [5]]) {
    const svg = G.sparkline(v, O);
    assert.ok(svg.includes("<polyline"));
    assert.ok(!svg.includes("NaN"), JSON.stringify(v));
  }
});

test("sparkline mantiene todos los puntos dentro del recuadro", () => {
  const svg = G.sparkline([1, 5, 2, 9], O);
  const puntos = /points="([^"]+)"/.exec(svg)[1].trim().split(/\s+/).map((p) => p.split(",").map(Number));
  assert.equal(puntos.length, 4);
  for (const [x, y] of puntos) {
    assert.ok(x >= 0 && x <= 100, `x=${x}`);
    assert.ok(y >= 0 && y <= 20, `y=${y}`);
  }
});

test("sparkline lleva una descripción accesible", () => {
  assert.match(G.sparkline([1, 2], { ...O, etiqueta: "Tiempo de ciclo" }), /role="img" aria-label="Tiempo de ciclo/);
});

test("barras dibuja un rectángulo por valor y da altura mínima a los valores positivos", () => {
  const svg = G.barras([0, 1, 100], O);
  assert.equal((svg.match(/<rect/g) || []).length, 3);
  const alturas = [...svg.matchAll(/height="([\d.]+)"/g)].map((m) => Number(m[1])).slice(1);
  assert.equal(alturas[0], 0, "el valor 0 no dibuja barra");
  assert.ok(alturas[1] >= 1, "el valor 1 de 100 debe verse");
  assert.ok(!svg.includes("NaN"));
});

test("barras con todo en cero no produce NaN", () => {
  assert.ok(!G.barras([0, 0, 0], O).includes("NaN"));
  assert.ok(!G.barras([], O).includes("NaN"));
});
