/* =========================================================
   main.js — Conecta Panel, UI y la fuente de datos
   ========================================================= */

Panel.alCambiar = (tipo, dato) => {
  UI.render(tipo, dato);
  if (tipo === "estado") actualizarBotones();
};

// Comandos panel -> ESP32. Hoy van al simulador; con hardware irán al servidor:
//   fetch("/api/comando", { method: "POST", body: JSON.stringify(cmd) })
Panel.enviar = (cmd) => {
  if (Panel.fuente === "simulador") setTimeout(() => Simulador.comando(cmd), 20);
};

/* ---------------- Pestañas ---------------- */
document.querySelectorAll(".pestana").forEach(btn => btn.addEventListener("click", () => irA(btn.dataset.vista)));
function irA(vista) {
  document.querySelectorAll(".pestana").forEach(b => b.classList.toggle("activa", b.dataset.vista === vista));
  document.querySelectorAll(".vista").forEach(v => v.classList.toggle("activa", v.id === "vista-" + vista));
}

/* ---------------- Tema ---------------- */
try { const t = localStorage.getItem("tema"); if (t) document.documentElement.dataset.theme = t; } catch {}
$("#btn-tema").addEventListener("click", () => {
  const actual = document.documentElement.dataset.theme
    || (matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light");
  const nuevo = actual === "dark" ? "light" : "dark";
  document.documentElement.dataset.theme = nuevo;
  try { localStorage.setItem("tema", nuevo); } catch {}
});

/* ---------------- Simulación ---------------- */
function actualizarBotones() {
  const s = Simulador;
  $("#btn-sim").textContent = !s.encendido ? "▶ Iniciar simulación" : s.pausado ? "▶ Reanudar" : "⏸ Pausar";
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
$("#btn-emergencia").addEventListener("click", () => { Simulador.emergencia(); actualizarBotones(); });
$("#btn-restablecer").addEventListener("click", () => { Simulador.restablecer(); actualizarBotones(); });
$("#btn-falla").addEventListener("click", () => toast(Simulador.inyectar($("#sel-falla").value)));
$("#rng-vel").addEventListener("input", (e) => {
  Simulador.velocidad = Number(e.target.value);
  $("#out-vel").textContent = e.target.value + "×";
});
$("#btn-limpiar").addEventListener("click", () => {
  if (!confirm("¿Borrar conteos, historial y alertas y detener la simulación?")) return;
  Simulador.apagar();
  Object.assign(Panel, { fuente: null, estadoMaquina: null, enlaceOk: false, pendientesEsp32: 0 });
  Panel.limpiar();
  actualizarBotones();
});

/* ---------------- Inventario ---------------- */
$("#inv-filtro-cat").addEventListener("change", (e) => { UI.filtroInv.cat = e.target.value; UI.inventario(); });
$("#inv-buscar").addEventListener("input", (e) => { UI.filtroInv.texto = e.target.value; UI.inventario(); });

/* ---------------- Etiquetas ---------------- */
$("#form-etiqueta").addEventListener("submit", (e) => {
  e.preventDefault();
  const f = new FormData(e.target);
  Panel.registrarEtiqueta(f.get("uid").trim(), f.get("nombre").trim(), f.get("categoria"));
  e.target.reset();
  toast("Etiqueta guardada. Recuerda sincronizar la tabla con el ESP32.");
});
$("#btn-sync").addEventListener("click", () => {
  if (!Panel.fuente) return toast("No hay ESP32 conectado. Inicia la simulación primero.");
  Panel.sincronizar();
});

/* ---------------- Historial y alertas ---------------- */
$("#his-filtro-cat").addEventListener("change", (e) => { UI.filtroHis = e.target.value; UI.historial(); });
$("#btn-csv").addEventListener("click", () => {
  if (!Panel.historial.length) return toast("El historial está vacío.");
  UI.exportarCSV();
});
$("#btn-reconocer").addEventListener("click", () => {
  Panel.alertas.forEach(a => (a.reconocida = true));
  UI.alertas();
});

/* ---------------- Botones generados dinámicamente ---------------- */
document.addEventListener("click", (e) => {
  const b = e.target.closest("button");
  if (!b) return;
  if (b.dataset.vaciar) Panel.vaciarContenedor(b.dataset.vaciar);
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

UI.todo();
actualizarBotones();
