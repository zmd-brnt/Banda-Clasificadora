/* =========================================================
   simulador.js — ESP32 virtual

   Imita el comportamiento descrito en el documento de avance:
   máquina de estados, lectura NFC con límite de 1.5 s, tabla local de
   categorías, contadores por contenedor, latido periódico, cola de eventos
   pendientes cuando la Raspberry Pi no responde, ACK y reenvío por seq.

   Se comunica con el panel solo a través de mensajes:
     ESP32 -> panel : Panel.recibir(msg)
     panel -> ESP32 : Simulador.comando(cmd)   (conectado en main.js)
   Cuando exista el hardware, este archivo se deja de cargar.
   ========================================================= */

const CANCELADO = Symbol("cancelado");

/* Tiempos base (ms) de una pieza a velocidad 1×. Ajustar con mediciones reales. */
const T = {
  transporte: 1600,    // entrada -> IR de inspección
  lecturaMin: 300,     // lectura NFC exitosa
  lecturaMax: 700,
  limiteNfc: 1500,     // documento: sin UID tras 1.5 s => rechazo
  servo: 400,          // movimiento de la compuerta
  salida: 1200,        // IR inspección -> IR salida
  aContenedor: 500,
  atasco: 2500,        // "IR bloqueado más de N segundos"
  separacionMin: 500,
  separacionMax: 1500,
  latido: 1000,
  enlaceCaido: 10000,
};

/* Etiquetas físicas que "existen" en el laboratorio simulado.
   Las 3 últimas NO están en la tabla maestra: sirven para probar el flujo de registro. */
const ETIQUETAS_FISICAS = [
  "04:A3:1F:B2:6C:80:00", "04:7B:22:C1:6C:80:00", "04:19:E0:5A:6C:80:00",
  "04:C4:93:0D:6C:80:00", "04:5E:71:F8:6C:80:00", "04:88:0A:3E:6C:80:00",
  "04:D2:6B:91:6C:80:00", "04:3F:A8:17:6C:80:00", "04:E6:52:C9:6C:80:00",
];

const Simulador = {
  encendido: false,
  pausado: false,
  detenido: false,          // ERROR o EMERGENCIA, requiere Restablecer
  velocidad: 1,
  token: 0,
  estado: "INACTIVO",

  seq: 0,
  tablaLocal: new Map(),    // uid -> categoria (copia de la tabla maestra)
  versionTabla: 1,
  sinAck: new Map(),        // seq -> msg, se borra al recibir ACK
  pendientes: [],           // mensajes PIEZA retenidos mientras el enlace está caído
  enlaceCaido: false,
  fallaSiguiente: null,
  contenido: { A: 0, B: 0, R: 0 },
  capacidad: { A: 15, B: 15, R: 15 },
  _latidoId: null,

  // ---------- Control ----------
  iniciar() {
    if (!this.encendido) {
      this.encendido = true;
      this._cargarTabla([...Panel.etiquetas.values()], Panel.versionTabla);
      this._latidoId = setInterval(() => this._latido(), T.latido);
      this._latido();
      this._arrancarCiclo();
    }
    this.pausado = false;   // el ciclo en curso detecta el cambio por sí solo
  },

  pausar() { this.pausado = true; },

  emergencia() {
    this._detener("EMERGENCIA", "Botón de paro presionado: motor detenido, servo al centro");
    this._transmitir({ tipo: "ALERTA", ts: Date.now(), nivel: "critico", codigo: "EMERGENCIA",
      mensaje: "Paro de emergencia activado. Motor detenido y compuerta al centro." });
  },

  restablecer() {
    this.detenido = false;
    UIBanda.reiniciar();
    this._transmitir({ tipo: "ALERTA", ts: Date.now(), nivel: "info", codigo: "RESTABLECIDO",
      mensaje: "El operador restableció el sistema." });
    this._arrancarCiclo();
  },

  apagar() {
    this.token++;
    clearInterval(this._latidoId);
    Object.assign(this, { encendido: false, pausado: false, detenido: false, estado: "INACTIVO",
      seq: 0, pendientes: [], enlaceCaido: false, fallaSiguiente: null });
    this.sinAck.clear();
    this.contenido = { A: 0, B: 0, R: 0 };
    UIBanda.reiniciar();
  },

  inyectar(falla) {
    switch (falla) {
      case "ENLACE":
        if (this.enlaceCaido) return "El enlace ya está caído.";
        this.enlaceCaido = true;
        setTimeout(() => this._restaurarEnlace(), T.enlaceCaido);
        return "Enlace UART desconectado durante 10 s. El ESP32 seguirá clasificando.";
      case "DUPLICADO": {
        const ultimo = [...this.sinAck.values()].pop() || this._ultimoEnviado;
        if (!ultimo) return "Aún no hay mensajes para duplicar.";
        this._transmitir({ ...ultimo });
        return `Se reenvió seq ${ultimo.seq} por segunda vez.`;
      }
      case "LLENO":
        // Atajo de simulación: se llena el contenedor A en ambos lados.
        this.contenido.A = this.capacidad.A;
        Panel.contenedores.A.contenido = Panel.contenedores.A.capacidad;
        Panel.alCambiar("contenedores");
        return "Contenedor A al límite. La próxima pieza A pausará la banda.";
      default:
        this.fallaSiguiente = falla;
        return "La falla se aplicará a la siguiente pieza.";
    }
  },

  /** Comandos que llegan desde el panel (Raspberry Pi -> ESP32). */
  comando(cmd) {
    if (this.enlaceCaido && cmd.cmd !== "ACK") {
      Panel.agregarAlerta("advertencia", "CMD_SIN_ENLACE", `No se pudo enviar ${cmd.cmd}: el enlace está caído.`);
      return;
    }
    switch (cmd.cmd) {
      case "ACK":
        this.sinAck.delete(cmd.seq);
        break;
      case "REENVIAR":
        cmd.seqs.forEach((s, i) => {
          const msg = this.sinAck.get(s);
          if (msg) setTimeout(() => this._transmitir({ ...msg, reenvio: true }), 300 + i * 150);
        });
        break;
      case "SYNC":
        this._cargarTabla(cmd.tabla, cmd.version);
        this._transmitir({ tipo: "ALERTA", ts: Date.now(), nivel: "info", codigo: "SYNC_OK",
          mensaje: `Tabla local del ESP32 actualizada: ${cmd.tabla.length} etiquetas (v${cmd.version}).` });
        this._latido();
        break;
      case "VACIADO":
        this.contenido[cmd.categoria] = 0;
        break;
    }
  },

  // ---------- Internos ----------
  _cargarTabla(tabla, version) {
    this.tablaLocal = new Map(tabla.map(e => [e.uid, e.categoria]));
    this.versionTabla = version;
  },

  _transmitir(msg) {
    if (msg.tipo === "PIEZA") {
      this.sinAck.set(msg.seq, msg);
      this._ultimoEnviado = msg;
    }
    if (this.enlaceCaido) {
      if (msg.tipo === "PIEZA") this.pendientes.push(msg);
      return;
    }
    Panel.recibir(msg);
  },

  _latido() {
    if (!this.encendido || this.enlaceCaido) return;
    Panel.recibir({ tipo: "LATIDO", ts: Date.now(), pendientes: this.pendientes.length, version_tabla: this.versionTabla });
  },

  _restaurarEnlace() {
    this.enlaceCaido = false;
    this._latido();
    const cola = this.pendientes.splice(0);
    cola.forEach((msg, i) => setTimeout(() => Panel.recibir({ ...msg, reenvio: true }), 200 + i * 150));
  },

  _estado(estado, detalle = "") {
    this.estado = estado;
    this._transmitir({ tipo: "ESTADO", ts: Date.now(), estado, detalle });
  },

  _detener(estado, detalle) {
    this.token++;
    this.detenido = true;
    UIBanda.motor(false);
    UIBanda.compuerta("centro");
    this._estado(estado, detalle);
  },

  _espera(ms, token) {
    return new Promise((ok, err) =>
      setTimeout(() => (this.token === token ? ok() : err(CANCELADO)), ms / this.velocidad));
  },

  async _hasta(condicion, token) {
    while (!condicion()) await this._espera(200 * this.velocidad, token);
  },

  _arrancarCiclo() {
    const token = ++this.token;
    this._ciclo(token).catch(e => { if (e !== CANCELADO) console.error(e); });
  },

  async _ciclo(token) {
    while (this.token === token) {
      if (this.pausado) {
        this._estado("PAUSA", "Banda en pausa");
        await this._hasta(() => !this.pausado, token);
      }
      await this._pieza(token);
      if (this.token !== token) return;   // la pieza terminó en ERROR
      this._estado("ESPERA", "Esperando la siguiente pieza");
      await this._espera(T.separacionMin + Math.random() * (T.separacionMax - T.separacionMin), token);
    }
  },

  _elegirPieza() {
    const falla = this.fallaSiguiente;
    this.fallaSiguiente = null;
    let uid;
    if (falla === "SIN_ETIQUETA") uid = null;
    else if (falla === "NO_REGISTRADA") {
      const fuera = ETIQUETAS_FISICAS.filter(u => !this.tablaLocal.has(u));
      uid = fuera.length ? fuera[Math.floor(Math.random() * fuera.length)] : "04:FF:00:00:6C:80:00";
    }
    else if (Math.random() < 0.07) uid = null;
    else uid = ETIQUETAS_FISICAS[Math.floor(Math.random() * ETIQUETAS_FISICAS.length)];
    return { uid, falla };
  },

  async _pieza(token) {
    const { uid, falla } = this._elegirPieza();
    let ciclo = 0;

    // 1. Transporte hasta el IR de inspección
    this._estado("TRANSPORTE", "Banda en movimiento");
    UIBanda.nuevaPieza();
    UIBanda.motor(true);
    UIBanda.moverPieza(34, "centro", T.transporte / this.velocidad);
    await this._espera(T.transporte, token);
    ciclo += T.transporte;

    // 2. IR detecta -> se detiene la banda -> lectura NFC (máx. 1.5 s)
    UIBanda.motor(false);
    UIBanda.sensor("sen-ir1", true);
    UIBanda.sensor("sen-nfc", true);
    this._estado("LECTURA_NFC", "Leyendo etiqueta…");
    if (falla === "JUNTAS") {
      this._transmitir({ tipo: "ALERTA", ts: Date.now(), nivel: "advertencia", codigo: "PIEZAS_JUNTAS",
        mensaje: "El IR de inspección se reactivó durante la clasificación: piezas muy juntas." });
    }
    const tLectura = uid ? T.lecturaMin + Math.random() * (T.lecturaMax - T.lecturaMin) : T.limiteNfc;
    await this._espera(tLectura, token);
    ciclo += tLectura;
    UIBanda.sensor("sen-nfc", false);

    // 3. Consulta a la tabla local
    let categoria, motivo = null;
    if (!uid) { categoria = "R"; motivo = "SIN_LECTURA"; }
    else if (this.tablaLocal.has(uid)) categoria = this.tablaLocal.get(uid);
    else { categoria = "R"; motivo = "NO_REGISTRADA"; }
    UIBanda.lectura(uid, categoria, motivo);

    // 4. Contenedor lleno -> pausa hasta confirmar vaciado
    if (this.contenido[categoria] >= this.capacidad[categoria]) {
      this._estado("PAUSA_CONTENEDOR", `Contenedor ${categoria} lleno`);
      this._transmitir({ tipo: "ALERTA", ts: Date.now(), nivel: "advertencia", codigo: "CONTENEDOR_LLENO",
        mensaje: `Contenedor ${CATEGORIAS[categoria].nombre} lleno. Banda en pausa hasta confirmar vaciado.` });
      await this._hasta(() => this.contenido[categoria] < this.capacidad[categoria], token);
    }

    // 5. Compuerta
    this._estado("CLASIFICANDO", `Compuerta → ${CATEGORIAS[categoria].servo}`);
    UIBanda.compuerta({ A: "izq", B: "der", R: "centro" }[categoria]);
    await this._espera(T.servo, token);
    ciclo += T.servo;
    UIBanda.sensor("sen-ir1", false);

    // 6. Reanuda y espera confirmación del IR de salida
    this._estado("CONFIRMA_SALIDA", "Esperando IR de salida");
    UIBanda.motor(true);
    if (falla === "ATASCO") {
      UIBanda.moverPieza(52, "centro", (T.salida * 0.4) / this.velocidad);
      await this._espera(T.salida * 0.4, token);
      UIBanda.motor(false);
      UIBanda.sensor("sen-ir2", false);
      await this._espera(T.atasco, token);
      this._detener("ERROR", "Atasco: el IR de salida no confirmó la pieza");
      this._transmitir({ tipo: "ALERTA", ts: Date.now(), nivel: "critico", codigo: "ATASCO",
        mensaje: "La pieza no llegó al IR de salida. Motor detenido, buzzer y LED rojo. Retira la pieza y presiona Restablecer." });
      return;
    }
    UIBanda.moverPieza(80, "centro", T.salida / this.velocidad);
    await this._espera(T.salida, token);
    ciclo += T.salida;
    UIBanda.sensor("sen-ir2", true);

    // 7. Cae al contenedor
    UIBanda.moverPieza(93, categoria, T.aContenedor / this.velocidad);
    await this._espera(T.aContenedor, token);
    ciclo += T.aContenedor;
    UIBanda.sensor("sen-ir2", false);
    UIBanda.ocultarPieza();
    UIBanda.compuerta("centro");
    this.contenido[categoria]++;

    // 8. Evento hacia la Raspberry Pi
    const msg = { tipo: "PIEZA", seq: ++this.seq, ts: Date.now(), uid, categoria, motivo, ciclo_ms: Math.round(ciclo) };
    if (falla === "HUECO") {
      this.sinAck.set(msg.seq, msg);   // se "pierde" en el cable, queda sin ACK
      this._ultimoEnviado = msg;
    } else {
      this._transmitir(msg);
    }
  },
};
