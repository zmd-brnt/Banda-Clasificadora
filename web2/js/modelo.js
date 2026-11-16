/* Almacén de datos del panel: lo llenan la fuente real o la demo y lo leen las vistas. */
(function (root) {
  const Triar = (root.Triar = root.Triar || {});
  const F = () => Triar.Formato;

  const MAX_PIEZAS = 5000;
  const MAX_FEED = 50;
  const CATEGORIAS = ["A", "B", "R"];

  function contenedoresVacios() {
    return { A: { contenido: 0, capacidad: 0 }, B: { contenido: 0, capacidad: 0 }, R: { contenido: 0, capacidad: 0 } };
  }

  function crear() {
    const oyentes = new Set();
    const claves = new Set();

    const m = {
      modo: null,            // "real" | "demo"
      servidorOk: true,      // el navegador alcanza a la Pi
      enlaceOk: false,       // la Pi recibe latidos del ESP32
      estado: null,
      piezas: [],
      alertas: [],
      etiquetas: [],
      contenedores: contenedoresVacios(),
      versionTabla: null,
      versionEsp32: null,
      pendientes: 0,
      ultimoLatidoTs: null,
      feed: [],

      suscribir(fn) {
        oyentes.add(fn);
        return () => oyentes.delete(fn);
      },

      aplicarInstantanea(inst) {
        m.enlaceOk = !!inst.enlace_ok;
        m.estado = inst.estado ?? null;
        m.versionTabla = inst.version_tabla ?? null;
        m.versionEsp32 = inst.version_esp32 ?? null;
        m.pendientes = inst.pendientes ?? 0;
        m.etiquetas = inst.etiquetas || [];
        m.alertas = inst.alertas || [];
        m.contenedores = { ...contenedoresVacios(), ...(inst.contenedores || {}) };
        claves.clear();
        m.piezas = [];
        for (const p of inst.piezas || []) agregarPieza(p);
        avisar();
      },

      aplicar(ev) {
        switch (ev.tipo) {
          case "PIEZA":
            if (!agregarPieza(ev)) return;
            empujarFeed(ev.ts, "PIEZA", textoPieza(ev), ev.categoria === "R" ? "aviso" : "ok");
            break;
          case "ESTADO":
            m.estado = ev.estado;
            empujarFeed(ev.ts, "ESTADO", `Estado: ${F().textoEstado(ev.estado)}`,
              (F().ESTADOS[ev.estado] || { tono: "neutro" }).tono);
            break;
          case "LATIDO":
            m.enlaceOk = true;
            m.pendientes = ev.pendientes ?? 0;
            m.versionEsp32 = ev.version_tabla ?? m.versionEsp32;
            m.ultimoLatidoTs = ev.ts ?? Date.now();
            break;
          case "ENLACE":
            m.enlaceOk = !!ev.ok;
            empujarFeed(ev.ts, "ENLACE", ev.ok ? "Enlace con el ESP32 restablecido" : "Sin latido del ESP32",
              ev.ok ? "ok" : "aviso");
            break;
          case "SERVIDOR":
            m.servidorOk = !!ev.ok;
            break;
          case "ALERTA":
            m.alertas = [{ ...ev, reconocida: !!ev.reconocida }, ...m.alertas];
            empujarFeed(ev.ts, "ALERTA", `Alerta: ${ev.mensaje}`,
              ev.nivel === "critico" ? "critico" : ev.nivel === "advertencia" ? "aviso" : "neutro");
            break;
          case "CONTENEDORES":
            m.contenedores = { ...contenedoresVacios(), ...ev.contenedores };
            break;
          case "ETIQUETAS":
            m.etiquetas = ev.etiquetas || [];
            m.versionTabla = ev.version ?? m.versionTabla;
            break;
          default:
            return;
        }
        avisar();
      },

      conteos() {
        const c = { total: m.piezas.length, A: 0, B: 0, R: 0, SIN_LECTURA: 0, NO_REGISTRADA: 0 };
        for (const p of m.piezas) {
          if (CATEGORIAS.includes(p.categoria)) c[p.categoria]++;
          if (p.motivo === "SIN_LECTURA" || p.motivo === "NO_REGISTRADA") c[p.motivo]++;
        }
        return c;
      },

      ultimaPieza() {
        return m.piezas.length ? m.piezas[m.piezas.length - 1] : null;
      },

      piezasPorMinuto(ahoraMs, minutos = 10) {
        const cubos = new Array(minutos).fill(0);
        for (const p of m.piezas) {
          const edad = Math.max(0, ahoraMs - p.ts);
          const atras = Math.floor(edad / 60000);
          if (atras < minutos) cubos[minutos - 1 - atras]++;
        }
        return cubos;
      },

      ciclosRecientes(n = 30) {
        return m.piezas.map((p) => p.ciclo_ms).filter((c) => typeof c === "number" && c > 0).slice(-n);
      },

      uidsSinRegistrar() {
        const registrados = new Set(m.etiquetas.map((e) => e.uid));
        const vistos = [];
        for (const p of m.piezas) {
          if (p.motivo === "NO_REGISTRADA" && p.uid && !registrados.has(p.uid) && !vistos.includes(p.uid)) {
            vistos.push(p.uid);
          }
        }
        return vistos;
      },

      marcarAlertasReconocidas() {
        m.alertas = m.alertas.map((a) => ({ ...a, reconocida: true }));
        avisar();
      },

      alertasSinReconocer() {
        return m.alertas.filter((a) => !a.reconocida).length;
      },

      porcentajeContenedor(cat) {
        const c = m.contenedores[cat];
        if (!c || !(c.capacidad > 0)) return 0;
        return Math.round(Math.min(100, Math.max(0, (c.contenido / c.capacidad) * 100)));
      },
    };

    function agregarPieza(p) {
      const clave = `${p.epoca}:${p.seq}`;
      if (claves.has(clave)) return false;
      claves.add(clave);
      m.piezas.push(p);
      m.piezas.sort((a, b) => a.epoca - b.epoca || a.seq - b.seq);
      if (m.piezas.length > MAX_PIEZAS) m.piezas.shift();
      return true;
    }

    function textoPieza(p) {
      if (p.motivo) return `Pieza #${p.seq} rechazada: ${F().etiquetaMotivo(p.motivo)}`;
      return `Pieza #${p.seq} → ${p.categoria === "R" ? "Rechazo" : p.categoria}`;
    }

    function empujarFeed(ts, tipo, texto, tono) {
      m.feed.unshift({ ts: ts ?? Date.now(), tipo, texto, tono });
      if (m.feed.length > MAX_FEED) m.feed.length = MAX_FEED;
    }

    function avisar() {
      for (const fn of [...oyentes]) fn();
    }

    return m;
  }

  Triar.Modelo = { crear };
  if (typeof module !== "undefined") module.exports = Triar.Modelo;
})(typeof window !== "undefined" ? window : globalThis);
