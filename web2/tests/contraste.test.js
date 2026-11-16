const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const css = fs.readFileSync(path.join(__dirname, "../css/tokens.css"), "utf8");

function bloque(selector) {
  const i = css.indexOf(selector);
  assert.ok(i >= 0, `no existe el bloque ${selector} en tokens.css`);
  const ini = css.indexOf("{", i);
  const fin = css.indexOf("}", ini);
  const vars = {};
  for (const m of css.slice(ini + 1, fin).matchAll(/(--[\w-]+)\s*:\s*(#[0-9a-fA-F]{6})/g)) vars[m[1]] = m[2];
  return vars;
}

function luminancia(hex) {
  const c = [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16) / 255)
    .map((x) => (x <= 0.03928 ? x / 12.92 : ((x + 0.055) / 1.055) ** 2.4));
  return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
}
function contraste(a, b) {
  const [x, y] = [luminancia(a), luminancia(b)].sort((p, q) => q - p);
  return (x + 0.05) / (y + 0.05);
}

const TEMAS = { oscuro: ":root {", claro: ':root[data-tema="claro"] {' };
const TEXTOS = ["--texto", "--texto-2", "--texto-3", "--acento", "--cat-a", "--cat-b", "--cat-r", "--ok", "--aviso", "--critico"];

for (const [tema, selector] of Object.entries(TEMAS)) {
  test(`tema ${tema}: los valores del spec están en tokens.css`, () => {
    const v = bloque(selector);
    const esperado = tema === "oscuro"
      ? { "--fondo": "#0E1417", "--superficie": "#172127", "--texto": "#E8EEF0", "--texto-2": "#9AA9B1", "--texto-3": "#8A9AA3",
          "--acento": "#38D6C8", "--sobre-acento": "#0E1417", "--cat-a": "#5AA9FF", "--cat-b": "#FF7AB6", "--cat-r": "#9AA9B1",
          "--ok": "#4ADE80", "--aviso": "#FBBF24", "--critico": "#FF6B5E" }
      : { "--fondo": "#F2F5F6", "--superficie": "#FFFFFF", "--texto": "#0E1417", "--texto-2": "#4B5A62", "--texto-3": "#5A6770",
          "--acento": "#00766E", "--sobre-acento": "#FFFFFF", "--cat-a": "#1F67C9", "--cat-b": "#B8185F", "--cat-r": "#5A6770",
          "--ok": "#167A32", "--aviso": "#8F5400", "--critico": "#C0342A" };
    for (const [k, valor] of Object.entries(esperado)) {
      assert.equal((v[k] || "").toUpperCase(), valor, `${tema} ${k}`);
    }
  });

  test(`tema ${tema}: todo texto y color de estado llega a 4.5:1 sobre superficie y fondo`, () => {
    const v = bloque(selector);
    for (const t of TEXTOS) {
      for (const base of ["--superficie", "--fondo"]) {
        const c = contraste(v[t], v[base]);
        assert.ok(c >= 4.5, `${tema}: ${t} ${v[t]} sobre ${base} ${v[base]} = ${c.toFixed(2)}`);
      }
    }
  });

  test(`tema ${tema}: el texto de los botones llega a 4.5:1 sobre su fondo`, () => {
    const v = bloque(selector);
    assert.ok(contraste(v["--sobre-acento"], v["--acento"]) >= 4.5, "sobre-acento");
    assert.ok(contraste(v["--sobre-critico"], v["--critico"]) >= 4.5, "sobre-critico");
  });
}
