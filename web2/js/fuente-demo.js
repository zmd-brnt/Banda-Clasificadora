/* Modo demo: genera los mismos mensajes que la Pi, con datos INVENTADOS, sin ESP32.
   Ningún valor de aquí es un resultado experimental. */
(function (root) {
  const Triar = (root.Triar = root.Triar || {});

  const ETIQUETAS_DEMO = [
    { uid: "04A31FB26C8000", nombre: "Llave", categoria: "A" },
    { uid: "04B2C4D6E80000", nombre: "Tornillo", categoria: "B" },
    { uid: "04C5D7E9F10000", nombre: "Pieza descartada", categoria: "R" },
  ];
  const UID_DESCONOCIDOS = ["04C1D2E3F40000", "04B7A9C8D10000"];
  const DURACION_MS = { ESPERA: 1500, LECTURA_NFC: 1200, CLASIFICANDO: 600, LIBERA_INSPECCION: 800,
    CONFIRMA_SALIDA: 600, RETORNO: 800 };
  const ESPERA_ENLACE_MS = 8000;
  const CATEGORIAS = ["A", "B", "R"];

  function normalizarUid(uid) {
    const limpio = String(uid || "").replace(/[:\s-]/g, "").toUpperCase();
    return /^[0-9A-F]{8,20}$/.test(limpio) && limpio.length % 2 === 0 ? limpio : null;
  }

  function crear(modelo, deps = {}) {
    const azar = deps.azar || Math.random;
    const ahora = deps.ahora || Date.now;
    const velocidad = deps.velocidad > 0 ? deps.velocidad : 1;
    const poner = deps.setTimeout || ((f, ms) => setTimeout(f, ms));
    const quitar = deps.clearTimeout || ((id) => clearTimeout(id));

    const s = {
      estado: "ESPERA", seq: 0, tabla: ETIQUETAS_DEMO.map((e) => ({ ...e })),
      contenedores: { A: { contenido: 0, capacidad: 15 }, B: { contenido: 0, capacidad: 15 }, R: { contenido: 0, capacidad: 15 } },
      version: 1, proxima: undefined, pieza: null, atasco: false, llenarProximo: false,
      enlaceCaidoHasta: null, detenido: false, timer: null, corriendo: false, idAlerta: 0,
    };

    const emitir = (ev) => modelo.aplicar({ ts: ahora(), ...ev });
    const ponerEstado = (estado) => { s.estado = estado; emitir({ tipo: "ESTADO", estado, detalle: "" }); };
    const alerta = (nivel, codigo, mensaje) => emitir({ tipo: "ALERTA", id: ++s.idAlerta, nivel, codigo, mensaje });
    const emitirContenedores = () => emitir({ tipo: "CONTENEDORES", contenedores: structuredClone(s.contenedores) });
    const emitirEtiquetas = () => emitir({ tipo: "ETIQUETAS", version: s.version, etiquetas: s.tabla.map((e) => ({ ...e })) });
    const enlaceCaido = () => s.enlaceCaidoHasta !== null;

    function elegirPieza() {
      if (s.proxima !== undefined) { const p = s.proxima; s.proxima = undefined; return p; }
      const r = azar();
      if (r < 0.06) return { uid: null };
      if (r < 0.12) return { uid: UID_DESCONOCIDOS[Math.floor(azar() * UID_DESCONOCIDOS.length)] };
      return { uid: s.tabla[Math.min(s.tabla.length - 1, Math.floor(r * s.tabla.length))].uid };
    }

    function clasificar(uid) {
      if (!uid) return { categoria: "R", motivo: "SIN_LECTURA", nombre: null };
      const e = s.tabla.find((x) => x.uid === uid);
      if (!e) return { categoria: "R", motivo: "NO_REGISTRADA", nombre: null };
      return { categoria: e.categoria, motivo: null, nombre: e.nombre };
    }

    function avanzar() {
      if (enlaceCaido() && ahora() >= s.enlaceCaidoHasta) {
        s.enlaceCaidoHasta = null;
        emitir({ tipo: "ENLACE", ok: true });
      }
      if (s.detenido || s.estado === "PAUSA_CONTENEDOR" || !DURACION_MS[s.estado]) return;
      if (!enlaceCaido()) emitir({ tipo: "LATIDO", pendientes: 0, version_tabla: s.version });

      switch (s.estado) {
        case "ESPERA":
          s.pieza = elegirPieza();
          ponerEstado("LECTURA_NFC");
          break;
        case "LECTURA_NFC": {
          const r = clasificar(s.pieza.uid);
          s.pieza.resultado = r;
          if (s.llenarProximo) {
            s.llenarProximo = false;
            s.contenedores[r.categoria].contenido = s.contenedores[r.categoria].capacidad;
            emitirContenedores();
          }
          const c = s.contenedores[r.categoria];
          if (c.contenido >= c.capacidad) {
            ponerEstado("PAUSA_CONTENEDOR");
            alerta("advertencia", "CONTENEDOR_LLENO", `El contenedor ${r.categoria} está lleno: vacíalo para continuar.`);
          } else {
            ponerEstado("CLASIFICANDO");
          }
          break;
        }
        case "CLASIFICANDO":
          ponerEstado("LIBERA_INSPECCION");
          break;
        case "LIBERA_INSPECCION":
          if (s.atasco) {
            s.atasco = false;
            s.detenido = true;
            ponerEstado("ERROR");
            alerta("critico", "ATASCO", "Atasco: la pieza no libera el sensor de inspección. Restablece la banda.");
          } else {
            ponerEstado("CONFIRMA_SALIDA");
          }
          break;
        case "CONFIRMA_SALIDA": {
          const r = s.pieza.resultado;
          s.seq += 1;
          s.contenedores[r.categoria].contenido += 1;
          emitir({ tipo: "PIEZA", epoca: 1, seq: s.seq, uid: s.pieza.uid, categoria: r.categoria, motivo: r.motivo,
            ciclo_ms: 4000 + Math.floor(azar() * 1500), reenvio: false, tardio: false, nombre: r.nombre });
          emitirContenedores();
          ponerEstado("RETORNO");
          break;
        }
        case "RETORNO":
          ponerEstado("ESPERA");
          break;
      }
    }

    function programar() {
      if (!s.corriendo) return;
      const espera = (DURACION_MS[s.estado] || 1000) / velocidad;
      s.timer = poner(() => { avanzar(); programar(); }, espera);
    }

    function iniciar() {
      modelo.modo = "demo";
      modelo.aplicarInstantanea({
        enlace_ok: true, estado: "ESPERA", version_tabla: s.version, version_esp32: s.version, pendientes: 0,
        etiquetas: s.tabla.map((e) => ({ ...e })), piezas: [], alertas: [], contenedores: structuredClone(s.contenedores), stats: {},
      });
      emitir({ tipo: "LATIDO", pendientes: 0, version_tabla: s.version });
      s.corriendo = true;
      programar();
    }

    function detener() {
      s.corriendo = false;
      if (s.timer !== null) quitar(s.timer);
    }

    function inyectar(falla) {
      switch (falla) {
        case "SIN_ETIQUETA": s.proxima = { uid: null }; break;
        case "NO_REGISTRADA": {
          const libres = UID_DESCONOCIDOS.filter((u) => !s.tabla.some((e) => e.uid === u));
          const uid = libres.length ? libres[Math.floor(azar() * libres.length)]
            : "04" + Math.floor(azar() * 0xffffff).toString(16).toUpperCase().padStart(6, "0") + "0000";
          s.proxima = { uid };
          break;
        }
        case "ATASCO": s.atasco = true; break;
        case "ENLACE_CAIDO":
          s.enlaceCaidoHasta = ahora() + ESPERA_ENLACE_MS;
          emitir({ tipo: "ENLACE", ok: false });
          break;
        case "CONTENEDOR_LLENO": s.llenarProximo = true; break;
      }
    }

    function restablecer() {
      if (!s.detenido) return;
      s.detenido = false;
      s.atasco = false;
      ponerEstado("ESPERA");
    }

    const ok = (mensaje = "enviado") => ({ ok: true, mensaje });
    const comandos = {
      async paro() {
        s.detenido = true;
        ponerEstado("EMERGENCIA");
        alerta("critico", "PARO_PANEL", "Se pidió un paro de emergencia desde el panel.");
        return ok();
      },
      async vaciado(categoria) {
        if (!CATEGORIAS.includes(categoria)) throw new Error("La categoría debe ser A, B o R.");
        s.contenedores[categoria].contenido = 0;
        emitirContenedores();
        if (s.estado === "PAUSA_CONTENEDOR") ponerEstado("CLASIFICANDO");
        return ok();
      },
      async capacidad(categoria, n) {
        if (!CATEGORIAS.includes(categoria)) throw new Error("La categoría debe ser A, B o R.");
        const cap = Number(n);
        if (!Number.isInteger(cap) || cap < 1 || cap > 999) throw new Error("La capacidad debe estar entre 1 y 999.");
        s.contenedores[categoria].capacidad = cap;
        emitirContenedores();
        return ok();
      },
      async sincronizar() { return ok(); },
      async registrarEtiqueta(uid, nombre, categoria) {
        const limpio = normalizarUid(uid);
        if (!limpio) throw new Error("UID inválido: usa hexadecimal, con o sin ':'.");
        const n = String(nombre || "").trim();
        if (!n || n.length > 60) throw new Error("El nombre es obligatorio (máximo 60 caracteres).");
        if (!CATEGORIAS.includes(categoria)) throw new Error("La categoría debe ser A, B o R.");
        const i = s.tabla.findIndex((e) => e.uid === limpio);
        if (i >= 0) s.tabla[i] = { uid: limpio, nombre: n, categoria }; else s.tabla.push({ uid: limpio, nombre: n, categoria });
        s.version += 1;
        emitirEtiquetas();
        emitir({ tipo: "LATIDO", pendientes: 0, version_tabla: s.version });
        return { ok: true, version: s.version };
      },
      async eliminarEtiqueta(uid) {
        const i = s.tabla.findIndex((e) => e.uid === uid);
        if (i < 0) throw new Error("Esa etiqueta no existe.");
        s.tabla.splice(i, 1);
        s.version += 1;
        emitirEtiquetas();
        emitir({ tipo: "LATIDO", pendientes: 0, version_tabla: s.version });
        return { ok: true, version: s.version };
      },
      async reconocerAlertas() { return { ok: true }; },
    };

    return { iniciar, detener, avanzar, inyectar, restablecer, comandos };
  }

  Triar.FuenteDemo = { crear, ETIQUETAS_DEMO };
  if (typeof module !== "undefined") module.exports = Triar.FuenteDemo;
})(typeof window !== "undefined" ? window : globalThis);
