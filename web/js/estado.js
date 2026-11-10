/* =========================================================
   estado.js — Modelo de datos del panel (lo que guardará la Raspberry Pi)

   Toda la información que llega del ESP32 entra por UNA sola función:
       Panel.recibir(mensaje)
   Dos fuentes posibles (main.js elige):
     - "simulador": el ESP32 virtual de simulador.js (sin servidor).
     - "uart": eventos en vivo (SSE /api/eventos) del servidor de la Raspberry Pi
       (rpi/servidor.py), que ya validó la trama, confirmó con ACK, descartó
       duplicados y guardó en SQLite.

   Tipos de mensaje (ya decodificados a JSON):
     PIEZA        { tipo, seq, ts, uid|null, categoria:"A"|"B"|"R", motivo|null, ciclo_ms,
                    reenvio, tardio?, epoca?, nombre? }
     ESTADO       { tipo, ts, estado, detalle }
     LATIDO       { tipo, ts, pendientes, version_tabla }
     ALERTA       { tipo, ts, nivel:"critico"|"advertencia"|"info", codigo, mensaje }
     CONTENEDORES { tipo, contenedores: { A:{contenido,capacidad}, B:…, R:… } }   (solo "uart")
     ETIQUETAS    { tipo, version, etiquetas:[…] }                               (solo "uart")
     ENLACE       { tipo, ok }                                                    (solo "uart")
   ========================================================= */

const CATEGORIAS = {
  A: { nombre: "Categoría A", servo: "izquierda" },
  B: { nombre: "Categoría B", servo: "derecha" },
  R: { nombre: "Rechazo",     servo: "centro" },
};

const MOTIVOS = {
  SIN_LECTURA:   "Sin lectura NFC (1.5 s)",
  NO_REGISTRADA: "UID no registrado",
};

const LATIDO_TIMEOUT_MS = 3000; // sin latido en este tiempo => enlace caído

const Panel = {
  // ---- Tabla maestra (persistirá en SQLite en la Raspberry Pi) ----
  etiquetas: new Map(),      // uid -> { uid, nombre, categoria, registrada }
  versionTabla: 1,           // se incrementa con cada cambio
  versionEsp32: 1,           // versión que el ESP32 reporta tener

  // ---- Datos recibidos ----
  historial: [],             // piezas ordenadas por seq
  seqVistos: new Set(),
  ultimoSeq: 0,
  faltantes: new Set(),      // seq que se pidió reenviar
  conteos: { A: 0, B: 0, R: 0, SIN_LECTURA: 0, NO_REGISTRADA: 0 },
  contenedores: {
    A: { capacidad: 15, contenido: 0 },
    B: { capacidad: 15, contenido: 0 },
    R: { capacidad: 15, contenido: 0 },
  },
  inventario: new Map(),     // uid -> { cantidad, ultima, categoria }
  desconocidos: new Map(),   // uid -> { veces, ultima }
  alertas: [],

  // ---- Estado en vivo ----
  estadoMaquina: null,
  detalleEstado: "",
  ultimoLatido: 0,
  pendientesEsp32: 0,
  enlaceOk: false,
  servidorOk: true,          // conexión SSE con la Raspberry Pi (solo "uart")
  fuente: null,              // "simulador" | "uart"

  // ---- Ganchos ----
  alCambiar: () => {},       // la UI se suscribe aquí
  enviar: () => {},          // comandos hacia el ESP32 (simulador o servidor)

  /** Punto único de entrada de datos. */
  recibir(msg) {
    switch (msg.tipo) {
      case "PIEZA":  this._pieza(msg); break;
      case "ESTADO": this._estado(msg); break;
      case "LATIDO": this._latido(msg); break;
      case "ALERTA": this.agregarAlerta(msg.nivel, msg.codigo, msg.mensaje, msg.ts); break;
      case "CONTENEDORES":
        for (const k in msg.contenedores) this.contenedores[k] = { ...msg.contenedores[k] };
        this.alCambiar("contenedores");
        break;
      case "ETIQUETAS":
        this.etiquetas = new Map(msg.etiquetas.map(e => [e.uid, e]));
        for (const uid of this.etiquetas.keys()) this.desconocidos.delete(uid);
        this.versionTabla = msg.version;
        this.alCambiar("etiquetas");
        break;
      case "ENLACE":
        this.enlaceOk = msg.ok;
        if (msg.ok) this.ultimoLatido = Date.now();
        this.alCambiar("latido", msg);
        break;
      default: console.warn("Mensaje desconocido", msg);
    }
  },

  _pieza(msg, silencioso = false) {
    // Con datos reales, el servidor ya confirmó (ACK), descartó duplicados y pidió los huecos.
    if (this.fuente === "uart") return this._piezaServidor(msg, silencioso);

    this._latido({ ts: msg.ts }, true);

    // 1) Duplicados: mismo seq ya registrado -> se descarta (pero se confirma)
    if (this.seqVistos.has(msg.seq)) {
      this.agregarAlerta("info", "UART_DUPLICADO", `Mensaje seq ${msg.seq} duplicado; se descartó.`);
      this.enviar({ cmd: "ACK", seq: msg.seq });
      return;
    }

    // 2) Huecos: llega un seq mayor al esperado -> pedir reenvío de los faltantes
    if (msg.seq > this.ultimoSeq + 1) {
      const faltan = [];
      for (let s = this.ultimoSeq + 1; s < msg.seq; s++) {
        if (!this.seqVistos.has(s)) { faltan.push(s); this.faltantes.add(s); }
      }
      if (faltan.length) {
        this.agregarAlerta("advertencia", "UART_HUECO",
          `Faltan mensajes seq ${faltan.join(", ")}; se solicitó reenvío.`);
        this.enviar({ cmd: "REENVIAR", seqs: faltan });
      }
    }
    const eraFaltante = this.faltantes.delete(msg.seq);

    this.seqVistos.add(msg.seq);
    this.ultimoSeq = Math.max(this.ultimoSeq, msg.seq);
    this.enviar({ cmd: "ACK", seq: msg.seq });

    const etiqueta = msg.uid ? this.etiquetas.get(msg.uid) : null;
    const registro = {
      ...msg,
      nombre: etiqueta ? etiqueta.nombre : null,
      recibido: Date.now(),
      tardio: eraFaltante || !!msg.reenvio,
    };

    // Inserta ordenado por seq (los reenvíos pueden llegar tarde)
    let i = this.historial.length;
    while (i > 0 && this.historial[i - 1].seq > msg.seq) i--;
    this.historial.splice(i, 0, registro);

    this.conteos[msg.categoria]++;
    if (msg.motivo) this.conteos[msg.motivo]++;
    this.contenedores[msg.categoria].contenido++;
    this._acumular(msg);
    this.alCambiar("pieza", registro);
  },

  /** Pieza que llega del servidor de la Raspberry Pi (o de la instantánea al cargar). */
  _piezaServidor(msg, silencioso) {
    const clave = `${msg.epoca || 1}:${msg.seq}`;
    if (this.seqVistos.has(clave)) return;   // p. ej. evento SSE que también vino en la instantánea
    this.seqVistos.add(clave);
    this.ultimoSeq = Math.max(this.ultimoSeq, msg.seq);
    if (!silencioso) this._latido({ ts: msg.ts }, true);

    const etiqueta = msg.uid ? this.etiquetas.get(msg.uid) : null;
    const registro = { ...msg, nombre: msg.nombre ?? (etiqueta ? etiqueta.nombre : null), tardio: !!msg.tardio };
    const antes = (a, b) => (a.epoca || 1) - (b.epoca || 1) || a.seq - b.seq;
    let i = this.historial.length;
    while (i > 0 && antes(this.historial[i - 1], registro) > 0) i--;
    this.historial.splice(i, 0, registro);

    this.conteos[msg.categoria]++;
    if (msg.motivo) this.conteos[msg.motivo]++;
    // La ocupación de los contenedores la reporta el ESP32 (mensaje CONTENEDORES)
    this._acumular(msg);
    if (!silencioso) this.alCambiar("pieza", registro);
  },

  /** Inventario por UID y lista de UID no registrados. */
  _acumular(msg) {
    if (msg.uid) {
      const inv = this.inventario.get(msg.uid) || { cantidad: 0, ultima: 0, categoria: msg.categoria };
      inv.cantidad++;
      inv.ultima = Math.max(inv.ultima, msg.ts);
      inv.categoria = msg.categoria;
      this.inventario.set(msg.uid, inv);
    }
    if (msg.motivo === "NO_REGISTRADA" && !this.etiquetas.has(msg.uid)) {
      const d = this.desconocidos.get(msg.uid) || { veces: 0, ultima: 0 };
      d.veces++; d.ultima = msg.ts;
      this.desconocidos.set(msg.uid, d);
    }
  },

  /** Carga la instantánea de GET /api/estado (modo "uart"). */
  cargarInstantanea(s) {
    this.fuente = "uart";
    this.historial = []; this.seqVistos.clear(); this.faltantes.clear(); this.ultimoSeq = 0;
    for (const k in this.conteos) this.conteos[k] = 0;
    this.inventario.clear(); this.desconocidos.clear();
    this.etiquetas = new Map(s.etiquetas.map(e => [e.uid, e]));
    this.versionTabla = s.version_tabla;
    this.versionEsp32 = s.version_esp32 ?? 0;
    s.piezas.forEach(p => this._piezaServidor({ tipo: "PIEZA", ...p }, true));
    this.alertas = s.alertas.map(a => ({ ...a }));
    for (const k in s.contenedores) this.contenedores[k] = { ...s.contenedores[k] };
    this.estadoMaquina = s.estado;
    this.enlaceOk = s.enlace_ok;
    this.pendientesEsp32 = s.pendientes;
    this.ultimoLatido = s.enlace_ok ? Date.now() : 0;
    this.servidorOk = true;
    this.alCambiar("todo");
  },

  _estado(msg) {
    this.estadoMaquina = msg.estado;
    this.detalleEstado = msg.detalle || "";
    this._latido({ ts: msg.ts }, true);
    this.alCambiar("estado", msg);
  },

  _latido(msg, silencioso = false) {
    this.ultimoLatido = Date.now();
    if (msg.pendientes !== undefined) this.pendientesEsp32 = msg.pendientes;
    if (msg.version_tabla !== undefined) this.versionEsp32 = msg.version_tabla;
    if (!this.enlaceOk) {
      this.enlaceOk = true;
      if (this.fuente !== "uart" && this.alertas.some(a => a.codigo === "ENLACE_CAIDO" && !a.cerrada)) {
        this.alertas.forEach(a => { if (a.codigo === "ENLACE_CAIDO") a.cerrada = true; });
        this.agregarAlerta("info", "ENLACE_OK", "Se restableció el latido del ESP32.");
      }
    }
    if (!silencioso) this.alCambiar("latido", msg);
  },

  /** Se llama periódicamente para detectar pérdida de latido. */
  revisarEnlace() {
    if (!this.fuente) return;
    const vivo = Date.now() - this.ultimoLatido < LATIDO_TIMEOUT_MS;
    if (this.enlaceOk && !vivo) {
      this.enlaceOk = false;
      if (this.fuente === "uart") {   // la alerta la genera y guarda el servidor
        this.alCambiar("latido", {});
        return;
      }
      this.agregarAlerta("critico", "ENLACE_CAIDO",
        "Sin latido del ESP32. Sigue clasificando con su tabla local; los eventos se recibirán al reconectar.");
      this.alCambiar("latido", {});
    }
  },

  agregarAlerta(nivel, codigo, mensaje, ts = Date.now()) {
    this.alertas.unshift({ id: crypto.randomUUID?.() || String(Math.random()), nivel, codigo, mensaje, ts, reconocida: false });
    this.alCambiar("alerta", this.alertas[0]);
  },

  // ---- Acciones del usuario ----
  /** "04:a3:1f…" -> "04A31F…". Devuelve null si no es hexadecimal válido (1 a 20 caracteres). */
  normalizarUid(uid) {
    const u = String(uid).replace(/[\s:-]/g, "").toUpperCase();
    return /^[0-9A-F]{1,20}$/.test(u) ? u : null;
  },

  registrarEtiqueta(uid, nombre, categoria) {
    uid = this.normalizarUid(uid);
    if (!uid) throw new Error("UID inválido: usa hexadecimal, con o sin ':'.");
    this.etiquetas.set(uid, { uid, nombre, categoria, registrada: Date.now() });
    this.desconocidos.delete(uid);
    this.versionTabla++;
    this.alCambiar("etiquetas");
  },

  eliminarEtiqueta(uid) {
    this.etiquetas.delete(uid);
    this.versionTabla++;
    this.alCambiar("etiquetas");
  },

  sincronizar() {
    this.enviar({ cmd: "SYNC", version: this.versionTabla, tabla: [...this.etiquetas.values()] });
  },

  vaciarContenedor(cat) {
    this.contenedores[cat].contenido = 0;
    this.enviar({ cmd: "VACIADO", categoria: cat });
    this.agregarAlerta("info", "VACIADO", `Contenedor ${CATEGORIAS[cat].nombre} marcado como vacío.`);
    this.alCambiar("contenedores");
  },

  limpiar() {
    this.historial = []; this.seqVistos.clear(); this.faltantes.clear(); this.ultimoSeq = 0;
    for (const k in this.conteos) this.conteos[k] = 0;
    for (const k in this.contenedores) this.contenedores[k].contenido = 0;
    this.inventario.clear(); this.desconocidos.clear(); this.alertas = [];
    this.alCambiar("todo");
  },

  // ---- Consultas para la UI ----
  get total() { return this.conteos.A + this.conteos.B + this.conteos.R; },

  cicloPromedio() {
    const ult = this.historial.slice(-20);
    if (!ult.length) return null;
    return ult.reduce((s, p) => s + p.ciclo_ms, 0) / ult.length;
  },

  ritmoPorMinuto() {
    const hace = Date.now() - 60000;
    return this.historial.filter(p => p.ts >= hace).length;
  },
};

/* Datos semilla (INVENTADOS) para ver la tabla maestra poblada en modo simulación.
   En modo "uart" se reemplazan por la tabla real de la Raspberry Pi. */
[
  ["04A31FB26C8000", "Tornillo M3",       "A"],
  ["047B22C16C8000", "Tuerca hexagonal",  "A"],
  ["0419E05A6C8000", "Arandela plana",    "A"],
  ["04C4930D6C8000", "Resistencia 1 kΩ",  "B"],
  ["045E71F86C8000", "Capacitor 100 µF",  "B"],
  ["04880A3E6C8000", "LED rojo 5 mm",     "B"],
].forEach(([uid, nombre, cat]) =>
  Panel.etiquetas.set(uid, { uid, nombre, categoria: cat, registrada: Date.now() - 86400000 }));
