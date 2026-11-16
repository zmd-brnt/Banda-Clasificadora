const test = require("node:test");
const assert = require("node:assert/strict");
require("../js/formato.js");
const Modelo = require("../js/modelo.js");
require("../js/etapas.js");
require("../js/graficas.js");
const Vistas = require("../js/vistas.js");

const AHORA = 10_000_000;
const pieza = (seq, extra = {}) => ({
  tipo: "PIEZA", epoca: 1, seq, ts: AHORA - 1000, uid: "04A31FB26C8000", categoria: "A",
  motivo: null, ciclo_ms: 4000, reenvio: false, tardio: false, nombre: "Llave", ...extra,
});
const conEstado = (estado) => { const m = Modelo.crear(); m.aplicar({ tipo: "ESTADO", estado }); return m; };
const cuenta = (html, re) => (html.match(re) || []).length;

test("la etapa activa sigue al estado del ESP32", () => {
  const html = Vistas.banda(conEstado("LECTURA_NFC"), { orientacion: "horizontal", ahora: AHORA });
  assert.match(html, /data-etapa="lector"[^>]*aria-current="step"/);
  assert.equal(cuenta(html, /aria-current="step"/g), 1);
  const esp = Vistas.banda(conEstado("ESPERA"), { orientacion: "horizontal", ahora: AHORA });
  assert.match(esp, /data-etapa="entrada"[^>]*aria-current="step"/);
});

test("la banda muestra las 5 etapas en el orden del montaje", () => {
  const html = Vistas.banda(conEstado("ESPERA"), { orientacion: "horizontal", ahora: AHORA });
  const orden = [...html.matchAll(/data-etapa="(\w+)"/g)].map((m) => m[1]);
  assert.deepEqual(orden, ["entrada", "inspeccion", "lector", "salida", "compuerta"]);
});

test("emergencia y pausa no marcan etapa y muestran el texto del estado", () => {
  for (const [estado, texto] of [["EMERGENCIA", "Emergencia"], ["PAUSA", "En pausa"]]) {
    const html = Vistas.banda(conEstado(estado), { orientacion: "vertical", ahora: AHORA });
    assert.equal(cuenta(html, /aria-current="step"/g), 0, estado);
    assert.ok(html.includes(texto), estado);
  }
});

test("la orientación se refleja en una clase", () => {
  assert.match(Vistas.banda(conEstado("ESPERA"), { orientacion: "vertical", ahora: AHORA }), /banda-v/);
  assert.match(Vistas.banda(conEstado("ESPERA"), { orientacion: "horizontal", ahora: AHORA }), /banda-h/);
});

test("el contenedor destino se resalta solo durante 2.5 s", () => {
  const m = Modelo.crear();
  m.aplicar(pieza(1, { ts: AHORA - 1000, categoria: "B" }));
  assert.match(Vistas.banda(m, { orientacion: "horizontal", ahora: AHORA }), /class="contenedor destino" data-cat="B"/);
  assert.ok(!/class="contenedor destino"/.test(Vistas.banda(m, { orientacion: "horizontal", ahora: AHORA + 3000 })));
});

test("los contenedores muestran ocupación y barra", () => {
  const m = Modelo.crear();
  m.aplicar({ tipo: "CONTENEDORES", contenedores: { A: { contenido: 7, capacidad: 15 }, B: { contenido: 15, capacidad: 15 }, R: { contenido: 0, capacidad: 15 } } });
  const html = Vistas.banda(m, { orientacion: "horizontal", ahora: AHORA });
  assert.ok(html.includes("7 de 15"));
  assert.ok(html.includes("--p:47%"));
  assert.match(html, /data-cat="B"[^>]*>[\s\S]*?Lleno/);
});

test("capacidad 0 o contenido mayor que la capacidad no rompen la barra", () => {
  const m = Modelo.crear();
  m.aplicar({ tipo: "CONTENEDORES", contenedores: { A: { contenido: 3, capacidad: 0 }, B: { contenido: 20, capacidad: 15 }, R: { contenido: 0, capacidad: 15 } } });
  const html = Vistas.banda(m, { orientacion: "horizontal", ahora: AHORA });
  assert.ok(html.includes("--p:0%"));
  assert.ok(html.includes("--p:100%"));
  assert.ok(!/NaN|undefined|null/.test(html));
});

test("un nombre con HTML se muestra como texto", () => {
  const m = Modelo.crear();
  m.aplicar(pieza(1, { nombre: "<img src=x onerror=alert(1)>" }));
  const html = Vistas.tiempoReal(m, { modo: "operacion", ahora: AHORA });
  assert.ok(html.includes("&lt;img src=x onerror=alert(1)&gt;"));
  assert.ok(!html.includes("<img"));
});

test("una alerta con HTML en el feed se muestra como texto", () => {
  const m = Modelo.crear();
  m.aplicar({ tipo: "ALERTA", id: 1, ts: AHORA, nivel: "critico", codigo: "X", mensaje: "<script>alert(1)</script>" });
  const html = Vistas.tiempoReal(m, { modo: "supervision", ahora: AHORA });
  assert.ok(!html.includes("<script>"));
  assert.ok(html.includes("&lt;script&gt;"));
});

test("en supervisión la vista no contiene ninguna acción ni gráficas", () => {
  const m = Modelo.crear();
  m.aplicar(pieza(1));
  const html = Vistas.tiempoReal(m, { modo: "supervision", ahora: AHORA });
  assert.ok(!html.includes("data-accion"));
  assert.ok(!html.includes("Piezas por minuto"));
  assert.match(html, /banda-v/);
});

test("en operación hay banda horizontal y las dos gráficas", () => {
  const m = Modelo.crear();
  m.aplicar(pieza(1));
  const html = Vistas.tiempoReal(m, { modo: "operacion", ahora: AHORA });
  assert.match(html, /banda-h/);
  assert.ok(html.includes("Piezas por minuto"));
  assert.ok(html.includes("Tiempo de ciclo"));
});

test("el modelo vacío muestra un estado vacío y ningún valor raro", () => {
  for (const modo of ["operacion", "supervision"]) {
    const html = Vistas.tiempoReal(Modelo.crear(), { modo, ahora: AHORA });
    assert.ok(html.includes("Aún no hay piezas"), modo);
    assert.ok(!/NaN|undefined|null/.test(html), modo);
  }
  assert.ok(Vistas.tiempoReal(Modelo.crear(), { modo: "operacion", ahora: AHORA, demo: true }).includes("La demostración"));
});

test("el resumen da el total, el ciclo promedio y el reparto por destino", () => {
  const m = Modelo.crear();
  m.aplicar(pieza(1, { ciclo_ms: 4000 }));
  m.aplicar(pieza(2, { categoria: "B", ciclo_ms: 5000 }));
  m.aplicar(pieza(3, { categoria: "R", motivo: "SIN_LECTURA", uid: null, ciclo_ms: null, nombre: null }));
  const html = Vistas.tiempoReal(m, { modo: "operacion", ahora: AHORA });
  assert.match(html, /class="resumen-total"[^>]*><span class="cifra-valor">3</);
  assert.ok(html.includes("4.5 s"));
  assert.match(html, /class="reparto" role="img" aria-label="Reparto: A 1, B 1, Rechazo 1"/);
  assert.equal(cuenta(html, /class="tramo tramo-[ABR]"/g), 3);
  assert.ok(html.includes("33 %"));
});

test("el reparto no dibuja tramos de categorías sin piezas y con 0 piezas no calcula porcentajes", () => {
  const m = Modelo.crear();
  m.aplicar(pieza(1));
  assert.equal(cuenta(Vistas.tiempoReal(m, { modo: "operacion", ahora: AHORA }), /class="tramo /g), 1);
  const vacio = Vistas.tiempoReal(Modelo.crear(), { modo: "supervision", ahora: AHORA });
  assert.equal(cuenta(vacio, /class="tramo /g), 0);
  assert.ok(!/NaN|undefined|null|Infinity/.test(vacio));
});

test("la última pieza es un ticket con el color de su destino y la línea de piezas recientes", () => {
  const m = Modelo.crear();
  m.aplicar(pieza(1, { categoria: "R", motivo: "SIN_LECTURA", nombre: null }));
  m.aplicar(pieza(2, { categoria: "B", nombre: "Tornillo" }));
  m.aplicar(pieza(3));
  const html = Vistas.tiempoReal(m, { modo: "operacion", ahora: AHORA });
  assert.match(html, /class="tarjeta ultima ticket" data-cat="A"/);
  const lista = /<ol class="recientes"[^>]*>([\s\S]*?)<\/ol>/.exec(html)[1];
  assert.deepEqual([...lista.matchAll(/cat-([ABR])/g)].map((x) => x[1]), ["R", "B", "A"]);
  assert.equal(cuenta(lista, /reciente-ultima/g), 1);
  assert.match(lista, /reciente-ultima[^>]*>A</);
  assert.match(html, /aria-label="Últimas piezas, la más reciente a la derecha"/);
});

test("la línea de piezas recientes muestra como máximo 12", () => {
  const m = Modelo.crear();
  for (let i = 1; i <= 20; i++) m.aplicar(pieza(i));
  const html = Vistas.tiempoReal(m, { modo: "supervision", ahora: AHORA });
  assert.equal(cuenta(html, /class="cat cat-A reciente/g), 12);
});

test("la última pieza rechazada explica el motivo", () => {
  const m = Modelo.crear();
  m.aplicar(pieza(1, { categoria: "R", motivo: "NO_REGISTRADA", nombre: null }));
  const html = Vistas.tiempoReal(m, { modo: "operacion", ahora: AHORA });
  assert.ok(html.includes("Etiqueta no registrada"));
});

test("el feed muestra 5 eventos en supervisión y hasta 12 en operación", () => {
  const m = Modelo.crear();
  for (let i = 0; i < 30; i++) m.aplicar({ tipo: "ESTADO", estado: i % 2 ? "ESPERA" : "LECTURA_NFC", ts: AHORA + i });
  assert.equal(cuenta(Vistas.tiempoReal(m, { modo: "supervision", ahora: AHORA }), /class="evento"/g), 5);
  assert.equal(cuenta(Vistas.tiempoReal(m, { modo: "operacion", ahora: AHORA }), /class="evento"/g), 12);
});

test("data-motor indica si la cinta avanza según el estado", () => {
  const motor = (estado) => /data-motor="(on|off)"/.exec(Vistas.banda(conEstado(estado), { orientacion: "horizontal", ahora: AHORA }))[1];
  for (const e of ["ESPERA", "LIBERA_INSPECCION", "CONFIRMA_SALIDA", "RETORNO"]) assert.equal(motor(e), "on", e);
  for (const e of ["LECTURA_NFC", "CLASIFICANDO", "PAUSA", "PAUSA_CONTENEDOR", "ERROR", "EMERGENCIA"]) assert.equal(motor(e), "off", e);
});

test("los estados que detienen la banda dicen qué hacer", () => {
  const ayuda = (estado) => Vistas.banda(conEstado(estado), { orientacion: "horizontal", ahora: AHORA });
  assert.ok(ayuda("ERROR").includes("Restablece la banda con el botón físico"));
  assert.ok(ayuda("EMERGENCIA").includes("Suelta el paro y restablece la banda con el botón físico"));
  assert.ok(ayuda("PAUSA").includes("Pulsa Restablecer en la banda para arrancar"));
  assert.ok(ayuda("PAUSA_CONTENEDOR").includes("Vacía el contenedor lleno"));
  assert.ok(!ayuda("ESPERA").includes("botón físico"));
});
