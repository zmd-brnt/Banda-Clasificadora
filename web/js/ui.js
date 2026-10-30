/* =========================================================
   ui.js — Dibujo del panel a partir de Panel (estado.js)
   No contiene lógica de negocio: solo lee Panel y pinta.
   ========================================================= */

const $ = (sel) => document.querySelector(sel);
const esc = (s) => String(s ?? "").replace(/[&<>"']/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
const hora = (ts) => new Date(ts).toLocaleTimeString("es-MX", { hour12: false });
const fechaHora = (ts) => new Date(ts).toLocaleString("es-MX", { hour12: false });
const pct = (n, total) => (total ? Math.round((n / total) * 100) : 0);

const NIVEL_TXT = { critico: "▲ Crítico", advertencia: "● Advertencia", info: "ⓘ Info" };
const ESTADO_TXT = {
  INACTIVO: "INACTIVO", ESPERA: "ESPERA", TRANSPORTE: "TRANSPORTE", LECTURA_NFC: "LECTURA NFC",
  CLASIFICANDO: "CLASIFICANDO", CONFIRMA_SALIDA: "CONFIRMA SALIDA", PAUSA: "PAUSA",
  PAUSA_CONTENEDOR: "CONTENEDOR LLENO", ERROR: "ERROR", EMERGENCIA: "EMERGENCIA",
};

function toast(texto) {
  const t = $("#toast");
  t.textContent = texto;
  t.hidden = false;
  clearTimeout(toast._id);
  toast._id = setTimeout(() => (t.hidden = true), 3500);
}

function pillCategoria(cat) {
  return `<span class="pill"><span class="muestra cat-${cat}"></span>${esc(CATEGORIAS[cat].nombre)}</span>`;
}

/* ---------------- Banda animada (solo en simulación) ---------------- */
const UIBanda = {
  TOP: { centro: 72, A: 22, R: 72, B: 120 },

  nuevaPieza() {
    const p = $("#pieza");
    p.className = "pieza";
    p.style.transitionDuration = "0ms";
    p.style.left = "2%";
    p.style.top = this.TOP.centro + "px";
    p.style.opacity = 1;
    p.hidden = false;
    void p.offsetWidth; // fuerza reflow para que la siguiente transición se anime
    $("#lectura-actual").textContent = "Pieza en camino al lector…";
  },
  moverPieza(leftPct, destino, ms) {
    const p = $("#pieza");
    p.style.transitionDuration = ms + "ms";
    p.style.left = leftPct + "%";
    p.style.top = this.TOP[destino] + "px";
  },
  ocultarPieza() { $("#pieza").hidden = true; },
  motor(on) { $("#banda").classList.toggle("moviendo", on); },
  sensor(id, on) { $("#" + id).classList.toggle("activo", on); },
  compuerta(dir) { $("#compuerta").className = "compuerta " + dir; },
  lectura(uid, cat, motivo) {
    $("#pieza").classList.add("cat-" + cat);
    const e = uid && Panel.etiquetas.get(uid);
    $("#lectura-actual").innerHTML = uid
      ? `UID <code>${esc(uid)}</code> ${e ? "· " + esc(e.nombre) : ""} → <b>${esc(CATEGORIAS[cat].nombre)}</b>${motivo ? " (" + esc(MOTIVOS[motivo]) + ")" : ""}`
      : `Sin etiqueta tras 1.5 s → <b>Rechazo</b>`;
  },
  reiniciar() {
    this.ocultarPieza(); this.motor(false); this.compuerta("centro");
    ["sen-ir1", "sen-nfc", "sen-ir2"].forEach(id => this.sensor(id, false));
    $("#lectura-actual").textContent = "Esperando pieza…";
  },
};

/* ---------------- Render ---------------- */
const UI = {
  filtroInv: { cat: "", texto: "" },
  filtroHis: "",

  render(tipo, dato) {
    switch (tipo) {
      case "pieza":
        this.feed(this.itemPieza(dato));
        this.kpis(); this.contenedores(); this.inventario(); this.historial(); this.desconocidos();
        break;
      case "alerta":
        this.feed(this.itemAlerta(dato));
        this.alertas();
        break;
      case "estado": this.estado(); break;
      case "latido": this.enlace(); this.etiquetas(); break;
      case "contenedores": this.contenedores(); break;
      case "etiquetas": this.etiquetas(); this.desconocidos(); this.inventario(); break;
      default: this.todo();
    }
  },

  todo() {
    this.kpis(); this.estado(); this.enlace(); this.contenedores(); this.inventario();
    this.etiquetas(); this.desconocidos(); this.historial(); this.alertas();
    if (!Panel.historial.length && !Panel.alertas.length) {
      $("#feed").innerHTML = `<li class="vacio">Aún no llegan eventos. Presiona <b>Iniciar simulación</b>.</li>`;
    }
  },

  kpis() {
    const c = Panel.conteos, total = Panel.total;
    $("#kpi-total").textContent = total;
    $("#kpi-A").textContent = c.A;
    $("#kpi-B").textContent = c.B;
    $("#kpi-R").textContent = c.R;
    $("#kpi-A-pct").textContent = `${pct(c.A, total)} % del total`;
    $("#kpi-B-pct").textContent = `${pct(c.B, total)} % del total`;
    $("#kpi-R-det").textContent = `${c.SIN_LECTURA} sin leer · ${c.NO_REGISTRADA} no registradas`;
    const ciclo = Panel.cicloPromedio();
    $("#kpi-ciclo").textContent = ciclo ? (ciclo / 1000).toFixed(2) + " s" : "—";
    $("#kpi-ritmo").textContent = `${Panel.ritmoPorMinuto()} piezas en el último minuto`;
    $("#nota-seq").textContent = `último seq ${Panel.ultimoSeq || "—"}`;

    // Barra apilada de distribución
    $("#barra-dist").innerHTML = ["A", "B", "R"].map(k =>
      `<div class="cat-${k}" style="flex-grow:${c[k]}" title="${CATEGORIAS[k].nombre}: ${c[k]} (${pct(c[k], total)} %)">${c[k] && pct(c[k], total) >= 8 ? c[k] : ""}</div>`
    ).join("");
    ["A", "B", "R"].forEach(k => ($("#cont-" + k).textContent = Panel.contenedores[k].contenido));
  },

  estado() {
    const e = Panel.estadoMaquina || "INACTIVO";
    const etq = $("#etq-estado");
    etq.textContent = ESTADO_TXT[e] || e;
    etq.className = "etq-estado" + (e === "ERROR" || e === "EMERGENCIA" ? " error" : e.startsWith("PAUSA") ? " pausa" : "");
    document.querySelectorAll("#pipeline li").forEach(li => li.classList.toggle("activo", li.dataset.estado === e));

    const chip = $("#chip-maquina");
    chip.className = "chip " + ({ ERROR: "critico", EMERGENCIA: "critico", PAUSA: "advertencia", PAUSA_CONTENEDOR: "advertencia", INACTIVO: "" }[e] ?? "ok");
    $("#txt-maquina").textContent = "ESP32: " + (ESTADO_TXT[e] || e);
  },

  enlace() {
    const chip = $("#chip-enlace");
    if (!Panel.fuente) { chip.className = "chip"; $("#txt-enlace").textContent = "Enlace: —"; return; }
    chip.className = "chip " + (Panel.enlaceOk ? "ok" : "critico");
    $("#txt-enlace").textContent = Panel.enlaceOk
      ? `Enlace UART OK${Panel.pendientesEsp32 ? ` · ${Panel.pendientesEsp32} pendientes` : ""}`
      : "Sin latido del ESP32";
    $("#chip-modo").className = "chip " + (Panel.fuente === "simulador" ? "advertencia" : "ok");
    $("#txt-modo").textContent = Panel.fuente === "simulador" ? "Modo simulación" : "Datos reales (UART)";
  },

  feed(html) {
    const ul = $("#feed");
    ul.querySelector(".vacio")?.remove();
    ul.insertAdjacentHTML("afterbegin", html);
    while (ul.children.length > 60) ul.lastElementChild.remove();
  },

  itemPieza(p) {
    const objeto = p.uid ? (p.nombre ? esc(p.nombre) : "Objeto desconocido") : "Sin etiqueta";
    const motivo = p.motivo ? ` · ${esc(MOTIVOS[p.motivo])}` : "";
    const tardio = p.tardio ? " · <b>recibido tarde (reenvío)</b>" : "";
    return `<li class="nuevo">
      <span class="marca-cat cat-${p.categoria}"></span>
      <span><b>${objeto}</b> → ${esc(CATEGORIAS[p.categoria].nombre)}</span>
      <span class="hora">${hora(p.ts)}</span>
      <span class="detalle">seq ${p.seq} · ${p.uid ? `<code>${esc(p.uid)}</code>` : "UID —"}${motivo} · ciclo ${(p.ciclo_ms / 1000).toFixed(2)} s${tardio}</span>
    </li>`;
  },

  itemAlerta(a) {
    return `<li class="nuevo">
      <span class="marca-cat sistema"></span>
      <span><span class="nivel ${a.nivel}">${NIVEL_TXT[a.nivel]}</span> ${esc(a.codigo)}</span>
      <span class="hora">${hora(a.ts)}</span>
      <span class="detalle">${esc(a.mensaje)}</span>
    </li>`;
  },

  contenedores() {
    $("#inv-contenedores").innerHTML = ["A", "B", "R"].map(k => {
      const c = Panel.contenedores[k];
      const p = Math.min(100, pct(c.contenido, c.capacidad));
      const nivel = p >= 100 ? "lleno" : p >= 80 ? "casi" : "";
      const estadoTxt = p >= 100 ? `<span class="nivel critico">▲ Lleno</span>`
        : p >= 80 ? `<span class="nivel advertencia">● Casi lleno</span>`
        : `<span class="estado-txt">✓ Con espacio</span>`;
      return `<article class="tarjeta contenedor-card cat-${k}">
        <h2>${pillCategoria(k)}</h2>
        <p class="valor">${Panel.conteos[k]} <span class="nota">piezas en total</span></p>
        <div class="medidor ${nivel}" title="${c.contenido} de ${c.capacidad}"><div style="width:${p}%"></div></div>
        <div class="fila-cont">
          <span class="nota">Contenedor: ${c.contenido} / ${c.capacidad}</span>
          ${estadoTxt}
          <button class="btn chico" data-vaciar="${k}">Marcar vaciado</button>
        </div>
        ${k === "R" ? `<p class="nota" style="margin-top:8px">${Panel.conteos.SIN_LECTURA} sin leer · ${Panel.conteos.NO_REGISTRADA} no registradas</p>` : ""}
      </article>`;
    }).join("");
    ["A", "B", "R"].forEach(k => ($("#cont-" + k).textContent = Panel.contenedores[k].contenido));
  },

  inventario() {
    const { cat, texto } = this.filtroInv;
    const t = texto.toLowerCase();
    const filas = [...Panel.inventario.entries()]
      .map(([uid, inv]) => ({ uid, ...inv, nombre: Panel.etiquetas.get(uid)?.nombre }))
      .filter(r => (!cat || r.categoria === cat) && (!t || r.uid.toLowerCase().includes(t) || (r.nombre || "").toLowerCase().includes(t)))
      .sort((a, b) => b.cantidad - a.cantidad);
    $("#inv-tabla").innerHTML = filas.length ? filas.map(r => `<tr>
        <td><code>${esc(r.uid)}</code></td>
        <td>${r.nombre ? esc(r.nombre) : `<span class="nota">No registrada</span>`}</td>
        <td>${pillCategoria(r.categoria)}</td>
        <td class="num">${r.cantidad}</td>
        <td>${hora(r.ultima)}</td>
      </tr>`).join("")
      : `<tr><td colspan="5" class="vacio">Sin piezas con etiqueta todavía.</td></tr>`;
  },

  etiquetas() {
    const lista = [...Panel.etiquetas.values()].sort((a, b) => a.categoria.localeCompare(b.categoria) || a.nombre.localeCompare(b.nombre));
    $("#tabla-etiquetas").innerHTML = lista.map(e => `<tr>
        <td><code>${esc(e.uid)}</code></td>
        <td>${esc(e.nombre)}</td>
        <td>${pillCategoria(e.categoria)}</td>
        <td>${fechaHora(e.registrada)}</td>
        <td><button class="btn chico" data-borrar="${esc(e.uid)}">Eliminar</button></td>
      </tr>`).join("") || `<tr><td colspan="5" class="vacio">Tabla vacía.</td></tr>`;
    $("#nota-etiquetas").textContent = `${lista.length} etiquetas · versión ${Panel.versionTabla}`;

    const sinc = Panel.versionEsp32 === Panel.versionTabla;
    $("#txt-sync").textContent = sinc
      ? `sincronizada (v${Panel.versionTabla})`
      : `desactualizada — ESP32 tiene v${Panel.versionEsp32}, panel v${Panel.versionTabla}`;
    $("#aviso-sync").classList.toggle("pendiente", !sinc);
    $("#ins-sync").hidden = sinc;
  },

  desconocidos() {
    const lista = [...Panel.desconocidos.entries()];
    $("#lista-desconocidos").innerHTML = lista.length ? lista.map(([uid, d]) => `<li>
        <span><code>${esc(uid)}</code><br><span class="nota">${d.veces} vez/veces · última ${hora(d.ultima)}</span></span>
        <button class="btn chico" data-registrar="${esc(uid)}">Registrar…</button>
      </li>`).join("") : `<li class="vacio">Ninguno por ahora.</li>`;
  },

  historial() {
    const filas = Panel.historial.filter(p => !this.filtroHis || p.categoria === this.filtroHis).slice(-300).reverse();
    $("#his-tabla").innerHTML = filas.length ? filas.map(p => `<tr>
        <td class="num">${p.seq}</td>
        <td>${hora(p.ts)}</td>
        <td>${p.uid ? `<code>${esc(p.uid)}</code>` : "—"}</td>
        <td>${p.nombre ? esc(p.nombre) : `<span class="nota">${p.uid ? "No registrada" : "Sin etiqueta"}</span>`}</td>
        <td>${pillCategoria(p.categoria)}</td>
        <td>${p.motivo ? esc(MOTIVOS[p.motivo]) : ""}</td>
        <td class="num">${(p.ciclo_ms / 1000).toFixed(2)} s</td>
        <td>${p.tardio ? "Reenviado" : "En vivo"}</td>
      </tr>`).join("") : `<tr><td colspan="8" class="vacio">Sin registros.</td></tr>`;
  },

  alertas() {
    const ul = $("#lista-alertas");
    ul.innerHTML = Panel.alertas.length ? Panel.alertas.map(a => `<li class="${a.reconocida ? "reconocida" : ""}">
        <span class="nivel ${a.nivel}">${NIVEL_TXT[a.nivel]}</span>
        <span><b>${esc(a.codigo)}</b><br>${esc(a.mensaje)}</span>
        <span class="hora nota">${hora(a.ts)}</span>
      </li>`).join("") : `<li class="vacio">Sin alertas.</li>`;
    const sinVer = Panel.alertas.filter(a => !a.reconocida && a.nivel !== "info").length;
    $("#ins-alertas").textContent = sinVer;
    $("#ins-alertas").hidden = !sinVer;
  },

  exportarCSV() {
    const enc = ["seq", "fecha_hora", "uid", "objeto", "categoria", "motivo", "ciclo_ms", "reenviado"];
    const filas = Panel.historial.map(p => [p.seq, new Date(p.ts).toISOString(), p.uid || "", p.nombre || "", p.categoria, p.motivo || "", p.ciclo_ms, p.tardio ? 1 : 0]);
    const csv = [enc, ...filas].map(f => f.map(v => `"${String(v).replace(/"/g, '""')}"`).join(",")).join("\n");
    const a = document.createElement("a");
    a.href = URL.createObjectURL(new Blob(["﻿" + csv], { type: "text/csv" }));
    a.download = `historial_banda_${new Date().toISOString().slice(0, 19).replace(/[:T]/g, "-")}.csv`;
    a.click();
    URL.revokeObjectURL(a.href);
  },
};
