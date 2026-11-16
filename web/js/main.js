/* =========================================================
   main.js — Conecta Panel, UI y la fuente de datos

   Fuente de datos:
     - "uart": la página la sirve rpi/servidor.py. Se carga GET api/estado y se
       escuchan los eventos en vivo de api/eventos (SSE). Los comandos van por
       POST api/comando y las etiquetas por api/etiquetas.
     - "simulador": la página se abre sin servidor (doble clic, python -m http.server)
       o con ?demo en la URL. Todo lo genera el ESP32 virtual de simulador.js.
   ========================================================= */

Panel.alCambiar = (tipo, dato) => {
  UI.render(tipo, dato);
  if (tipo === "estado" || tipo === "todo") actualizarBotones();
};

// Comandos panel -> ESP32 en modo simulación
Panel.enviar = (cmd) => {
  if (Panel.fuente === "simulador") setTimeout(() => Simulador.comando(cmd), 20);
};

const esUart = () => Panel.fuente === "uart";

/* ---------------- API del servidor (modo "uart") ---------------- */
async function api(metodo, ruta, datos) {
  const r = await fetch(ruta, {
    method: metodo,
    headers: datos ? { "Content-Type": "application/json" } : {},
    body: datos ? JSON.stringify(datos) : undefined,
  });
  let cuerpo = {};
  try { cuerpo = await r.json(); } catch {}
  if (!r.ok) throw new Error(cuerpo.error || cuerpo.mensaje || `Error ${r.status}`);
  return cuerpo;
}

async function comandoServidor(cmd) {
  try {
    const r = await api("POST", "api/comando", cmd);
    if (r.mensaje && r.mensaje !== "enviado") toast(r.mensaje);
    return true;
  } catch (e) {
    toast("No se pudo enviar " + cmd.cmd + ": " + e.message);
    return false;
  }
}

let fuenteEventos = null;

async function cargarDelServidor() {
  const r = await fetch("api/estado", { cache: "no-store" });
  if (!r.ok) throw new Error("sin servidor");
  const instantanea = await r.json();
  if (instantanea.modo !== "uart") throw new Error("respuesta inesperada");
  Panel.cargarInstantanea(instantanea);
}

function abrirEventos() {
  fuenteEventos = new EventSource("api/eventos");
  let reconectando = false;
  fuenteEventos.onmessage = (e) => Panel.recibir(JSON.parse(e.data));
  fuenteEventos.onerror = () => {
    Panel.servidorOk = false;
    reconectando = true;
    UI.enlace();
  };
  fuenteEventos.onopen = async () => {
    Panel.servidorOk = true;
    if (reconectando) {   // se pudieron perder eventos mientras no habia conexion
      reconectando = false;
      try { await cargarDelServidor(); } catch {}
    }
    UI.enlace();
  };
}

function modoUart() {
  // Controles de simulación fuera; queda solo el paro por software
  ["#btn-sim", "#btn-restablecer", "#grp-fallas", "#grp-vel"].forEach(s => ($(s).hidden = true));
  $("#nota-uart").hidden = false;
  $("#pie-fuente").innerHTML = "Datos reales: <b>ESP32 por UART</b>, guardados en la Raspberry Pi (SQLite).";

  Panel.enviar = (cmd) => {
    if (cmd.cmd === "ACK" || cmd.cmd === "REENVIAR") return;   // los resuelve el servidor
    comandoServidor(cmd);
  };
  Panel.registrarEtiqueta = async (uid, nombre, categoria) => {
    const normal = Panel.normalizarUid(uid);
    if (!normal) throw new Error("UID inválido: usa hexadecimal, con o sin ':'.");
    await api("POST", "api/etiquetas", { uid: normal, nombre, categoria });
    // La tabla se actualiza con el evento ETIQUETAS y el servidor envía SYNC al ESP32
  };
  Panel.eliminarEtiqueta = async (uid) => {
    try { await api("DELETE", "api/etiquetas/" + encodeURIComponent(uid)); }
    catch (e) { toast(e.message); }
  };
  Panel.sincronizar = () => comandoServidor({ cmd: "SYNC" });
  Panel.vaciarContenedor = async (cat) => {
    // La ocupación se actualiza cuando el ESP32 responde con CONTENEDORES
    if (await comandoServidor({ cmd: "VACIADO", categoria: cat })) toast(`Se pidió vaciar el contenedor ${cat}.`);
  };
  abrirEventos();
}

/* ---------------- Pestañas ---------------- */
document.querySelectorAll(".pestana").forEach(btn => btn.addEventListener("click", () => irA(btn.dataset.vista)));
function irA(vista) {
  document.querySelectorAll(".pestana").forEach(b => b.classList.toggle("activa", b.dataset.vista === vista));
  document.querySelectorAll(".vista").forEach(v => v.classList.toggle("activa", v.id === "vista-" + vista));
}

/* ---------------- Tema ---------------- */
try { const t = localStorage.getItem("tema"); if (t) document.documentElement.dataset.theme = t; } catch {}
$("#btn-tema").addEventListener("click", () => {
  const actual = document.documentElement.dataset.theme || "dark";   // el panel es oscuro por defecto
  const nuevo = actual === "dark" ? "light" : "dark";
  document.documentElement.dataset.theme = nuevo;
  try { localStorage.setItem("tema", nuevo); } catch {}
});

/* ---------------- Barra superior ---------------- */
function actualizarBotones() {
  if (esUart()) {
    $("#btn-emergencia").disabled = Panel.estadoMaquina === "EMERGENCIA";
    return;
  }
  const s = Simulador;
  const sim = $("#btn-sim");
  sim.dataset.modo = !s.encendido ? "iniciar" : s.pausado ? "reanudar" : "pausar";
  sim.querySelector(".lbl").textContent = !s.encendido ? "Iniciar simulación" : s.pausado ? "Reanudar" : "Pausar";
  $("#btn-emergencia").disabled = !s.encendido || s.detenido;
  $("#btn-restablecer").disabled = !s.detenido;
  $("#btn-falla").disabled = !s.encendido || s.detenido;
}

$("#btn-sim").addEventListener("click", () => {
  if (!Simulador.encendido || Simulador.pausado) {
    Panel.fuente = "simulador";
    Simulador.iniciar();
  } else {
    Simulador.pausar();
    toast("La banda se pausará al terminar la pieza actual.");
  }
  actualizarBotones();
});
$("#btn-emergencia").addEventListener("click", () => {
  if (esUart()) {
    if (confirm("¿Enviar PARO al ESP32? Es un paro por software: el paro seguro es el botón físico.")) {
      Panel.enviar({ cmd: "PARO" });
    }
    return;
  }
  Simulador.emergencia();
  actualizarBotones();
});
$("#btn-restablecer").addEventListener("click", () => { Simulador.restablecer(); actualizarBotones(); });
$("#btn-falla").addEventListener("click", () => toast(Simulador.inyectar($("#sel-falla").value)));
$("#rng-vel").addEventListener("input", (e) => {
  Simulador.velocidad = Number(e.target.value);
  $("#out-vel").textContent = e.target.value + "×";
});
$("#btn-limpiar").addEventListener("click", () => {
  if (esUart()) return toast("Con datos reales el historial vive en la Raspberry Pi y no se borra desde aquí.");
  if (!confirm("¿Borrar conteos, historial y alertas y detener la simulación?")) return;
  Simulador.apagar();
  Object.assign(Panel, { fuente: null, estadoMaquina: null, enlaceOk: false, pendientesEsp32: 0 });
  Panel.limpiar();
  actualizarBotones();
});

/* ---------------- Inventario ---------------- */
$("#inv-filtro-cat").addEventListener("change", (e) => { UI.filtroInv.cat = e.target.value; UI.inventario(); });
$("#inv-buscar").addEventListener("input", (e) => { UI.filtroInv.texto = e.target.value; UI.inventario(); });

function cambiarCapacidad(cat) {
  const n = Number($("#cap-" + cat).value);
  if (!Number.isInteger(n) || n < 1 || n > 999) return toast("La capacidad debe ser un entero entre 1 y 999.");
  if (esUart()) {
    Panel.enviar({ cmd: "CAPACIDAD", categoria: cat, capacidad: n });
    toast(`Capacidad de ${cat} enviada al ESP32 (${n}).`);
  } else {
    Panel.contenedores[cat].capacidad = n;
    Panel.enviar({ cmd: "CAPACIDAD", categoria: cat, capacidad: n });
    UI.contenedores();
  }
}

/* ---------------- Etiquetas ---------------- */
$("#form-etiqueta").addEventListener("submit", async (e) => {
  e.preventDefault();
  const f = new FormData(e.target);
  try {
    await Panel.registrarEtiqueta(f.get("uid").trim(), f.get("nombre").trim(), f.get("categoria"));
  } catch (err) {
    return toast(err.message);
  }
  e.target.reset();
  toast(esUart() ? "Etiqueta guardada; la tabla se envía sola al ESP32."
                 : "Etiqueta guardada. Recuerda sincronizar la tabla con el ESP32.");
});
$("#btn-sync").addEventListener("click", () => {
  if (!Panel.fuente) return toast("No hay ESP32 conectado. Inicia la simulación primero.");
  Panel.sincronizar();
});

/* ---------------- Historial y alertas ---------------- */
$("#his-filtro-cat").addEventListener("change", (e) => { UI.filtroHis = e.target.value; UI.historial(); });
$("#btn-csv").addEventListener("click", () => {
  if (esUart()) { location.href = "api/historial.csv"; return; }   // historial completo de SQLite
  if (!Panel.historial.length) return toast("El historial está vacío.");
  UI.exportarCSV();
});
$("#btn-reconocer").addEventListener("click", () => {
  Panel.alertas.forEach(a => (a.reconocida = true));
  if (esUart()) api("POST", "api/alertas/reconocer", {}).catch(e => toast(e.message));
  UI.alertas();
});

/* ---------------- Botones generados dinámicamente ---------------- */
document.addEventListener("click", (e) => {
  const b = e.target.closest("button");
  if (!b) return;
  if (b.dataset.vaciar) Panel.vaciarContenedor(b.dataset.vaciar);
  if (b.dataset.capacidad) cambiarCapacidad(b.dataset.capacidad);
  if (b.dataset.borrar && confirm(`¿Eliminar la etiqueta ${b.dataset.borrar}?`)) Panel.eliminarEtiqueta(b.dataset.borrar);
  if (b.dataset.registrar) {
    const form = $("#form-etiqueta");
    form.uid.value = b.dataset.registrar;
    irA("etiquetas");
    form.nombre.focus();
  }
});

/* ---------------- Temporizadores ---------------- */
setInterval(() => Panel.revisarEnlace(), 500);
setInterval(() => UI.kpis(), 5000);

/* ---------------- Arranque: ¿hay servidor? ---------------- */
(async () => {
  const forzarDemo = new URLSearchParams(location.search).has("demo");
  if (location.protocol !== "file:" && !forzarDemo) {
    try {
      await cargarDelServidor();
      modoUart();
      actualizarBotones();
      return;
    } catch {
      // Sin servidor de la Pi: se queda en modo simulación
    }
  }
  UI.todo();
  actualizarBotones();
})();
