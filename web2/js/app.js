/* Arranque de Triar. Es el único archivo que toca el DOM: elige la fuente de datos (la Pi o el demo),
   pinta las vistas, atiende pestañas y acciones y respeta el modo de pantalla (ancha / angosta). */
(function () {
  const T = window.Triar;
  const { Modelo, FuenteReal, FuenteDemo, Modo, Vistas, Formato } = T;
  const params = new URLSearchParams(location.search);

  const modelo = Modelo.crear();
  const vistaInicial = params.get("vista");
  const estado = { vista: ["tiempo-real", "inventario", "etiquetas", "historial", "alertas"].includes(vistaInicial) ? vistaInicial : "tiempo-real", modo: Modo.deAncho(window.innerWidth), filtro: "todas", comandos: null, demo: null };
  const ultimo = { encabezado: "", pestanas: "", contenido: "" };
  let pendiente = false;      // hay un repintado esperando a que se suelte un campo de formulario
  let animacion = 0;
  let anunciado = { estado: null, enlace: null };

  const $ = (sel) => document.querySelector(sel);
  const VISTAS = { "tiempo-real": "tiempoReal", inventario: "inventario", etiquetas: "etiquetas", historial: "historial", alertas: "alertas" };

  /* ------------------------------------------------------------ pintado */
  function contenidoDe(vista) {
    const fn = Vistas[VISTAS[vista]];
    if (typeof fn !== "function") return '<p class="secundario">Esta sección se está construyendo.</p>';
    return fn(modelo, { modo: estado.modo, ahora: Date.now(), filtro: estado.filtro, demo: modelo.modo === "demo" });
  }

  function claveFoco(el) {
    if (!el || el === document.body) return null;
    if (el.id) return `#${el.id}`;
    const d = el.dataset || {};
    if (d.vista) return `[data-vista="${d.vista}"]`;
    if (d.accion) {
      return `[data-accion="${d.accion}"]` + (d.categoria ? `[data-categoria="${d.categoria}"]` : "") + (d.uid ? `[data-uid="${d.uid}"]` : "");
    }
    return null;
  }

  function poner(region, html, clave) {
    if (ultimo[clave] === html) return;
    ultimo[clave] = html;
    const activo = document.activeElement;
    const sel = region.contains(activo) ? claveFoco(activo) : null;
    region.innerHTML = html;
    if (sel) { const nuevo = region.querySelector(sel); if (nuevo) nuevo.focus({ preventScroll: true }); }
  }

  function pintar() {
    animacion = 0;
    const ahora = Date.now();
    document.body.dataset.modo = estado.modo;
    const contenido = $("#contenido");
    const activo = document.activeElement;
    if (contenido.contains(activo) && /^(INPUT|SELECT|TEXTAREA)$/.test(activo.tagName)) {
      pendiente = true;                       // no se pisa lo que la persona está escribiendo
    } else {
      pendiente = false;
      poner(contenido, contenidoDe(estado.vista), "contenido");
    }
    poner($("#encabezado"), Vistas.encabezado(modelo, { modo: estado.modo, ahora }), "encabezado");
    poner($("#pestanas"), Vistas.pestanas(estado.modo, { activa: estado.vista, alertas: modelo.alertasSinReconocer() }), "pestanas");
    $("#pie").textContent = location.protocol === "file:"
      ? "Abierto desde un archivo: solo funciona el modo demostración."
      : `Panel en http://${location.host}`;
    anunciarCambios();
  }

  function programarPintado() {
    if (!animacion) animacion = requestAnimationFrame(pintar);
  }

  /* Anuncios para lectores de pantalla: solo lo importante, nunca cada pieza */
  function anunciarCambios() {
    const info = Formato.ESTADOS[modelo.estado];
    const importante = info && (info.tono === "critico" || info.tono === "aviso");
    let texto = "";
    if (modelo.estado !== anunciado.estado && importante) texto = `Estado: ${Formato.textoEstado(modelo.estado)}`;
    else if (modelo.enlaceOk !== anunciado.enlace && anunciado.enlace !== null) {
      texto = modelo.enlaceOk ? "Enlace con el ESP32 restablecido" : "Sin latido del ESP32";
    }
    anunciado = { estado: modelo.estado, enlace: modelo.enlaceOk };
    if (texto) $("#anuncio").textContent = texto;
  }

  function avisar(texto, tono) {
    const el = document.createElement("div");
    el.className = "aviso";
    if (tono === "critico") el.style.borderLeftColor = "var(--critico)";
    el.textContent = texto;
    $("#avisos").appendChild(el);
    setTimeout(() => el.remove(), 6000);
  }

  /* ----------------------------------------------------------- acciones */
  async function ejecutar(fn, exito) {
    try {
      const r = await fn();
      const texto = r && r.mensaje && r.mensaje !== "enviado" ? r.mensaje : exito;
      if (texto) avisar(texto);
      return r;
    } catch (e) {
      avisar(e.message || "Algo salió mal.", "critico");
      return null;
    }
  }

  const ACCIONES = {
    paro: () => ejecutar(() => estado.comandos.paro(), "Paro enviado."),
    vaciado: (el) => ejecutar(() => estado.comandos.vaciado(el.dataset.categoria), `Se pidió vaciar el contenedor ${el.dataset.categoria}.`),
    capacidad: (el) => {
      const cat = el.dataset.categoria;
      const campo = document.querySelector(`input[data-capacidad="${cat}"]`);
      return ejecutar(() => estado.comandos.capacidad(cat, Number(campo && campo.value)), `Capacidad del contenedor ${cat} actualizada.`);
    },
    sincronizar: () => ejecutar(() => estado.comandos.sincronizar(), "Se envió la tabla al ESP32."),
    "eliminar-etiqueta": (el) => {
      if (!confirm(`¿Eliminar la etiqueta ${el.dataset.nombre || el.dataset.uid}?`)) return null;
      return ejecutar(() => estado.comandos.eliminarEtiqueta(el.dataset.uid), "Etiqueta eliminada.");
    },
    "reconocer-alertas": async () => {
      const r = await ejecutar(() => estado.comandos.reconocerAlertas());
      if (r) modelo.marcarAlertasReconocidas();
    },
  };

  document.addEventListener("click", (e) => {
    const pestana = e.target.closest("[data-vista]");
    if (pestana) {
      estado.vista = pestana.dataset.vista;
      pintar();
      return;
    }
    const rellenar = e.target.closest("[data-rellenar-uid]");
    if (rellenar && Modo.puede(estado.modo, "registrar-etiqueta")) {
      const campo = $("#campo-uid");
      if (campo) { campo.value = rellenar.dataset.rellenarUid; const n = $("#campo-nombre"); if (n) n.focus(); }
      return;
    }
    const el = e.target.closest("[data-accion]");
    if (!el || el.tagName === "FORM") return;
    const accion = el.dataset.accion;
    if (!Modo.puede(estado.modo, accion)) { e.preventDefault(); return; }   // pantalla angosta: nada se ejecuta
    if (accion === "exportar-csv") return;                                   // es un enlace de descarga
    const fn = ACCIONES[accion];
    if (fn && estado.comandos) fn(el);
  });

  document.addEventListener("submit", (e) => {
    const form = e.target.closest("[data-accion]");
    if (!form) return;
    e.preventDefault();
    if (!Modo.puede(estado.modo, form.dataset.accion) || !estado.comandos) return;
    if (form.dataset.accion === "registrar-etiqueta") {
      const datos = new FormData(form);
      ejecutar(() => estado.comandos.registrarEtiqueta(datos.get("uid"), datos.get("nombre"), datos.get("categoria")),
        "Etiqueta registrada.").then((r) => {
        if (!r) return;
        form.reset();
        // Si se envió con Enter el campo sigue enfocado y la tabla no se repintaría: se suelta el foco.
        if (form.contains(document.activeElement)) document.activeElement.blur();
        pintar();
      });
    }
  });

  document.addEventListener("change", (e) => {
    if (e.target.matches("[data-filtro]")) { estado.filtro = e.target.value; pintar(); }
  });
  $("#contenido").addEventListener("focusout", () => { if (pendiente) setTimeout(pintar, 0); });

  /* ---------------------------------------------------------- modo y arranque */
  const ancha = window.matchMedia("(min-width: 900px)");
  function cambiarModo() {
    estado.modo = ancha.matches ? "operacion" : "supervision";
    if (estado.modo === "supervision" && estado.vista === "etiquetas") estado.vista = "tiempo-real";
    pintar();
  }
  if (ancha.addEventListener) ancha.addEventListener("change", cambiarModo);

  async function arrancar() {
    modelo.suscribir(programarPintado);
    cambiarModo();
    setInterval(pintar, 1000);
    const forzarDemo = params.has("demo") || location.protocol === "file:";
    if (!forzarDemo) {
      try {
        await FuenteReal.conectar(modelo, { fetch: window.fetch.bind(window), EventSource: window.EventSource });
        estado.comandos = FuenteReal.comandos({ fetch: window.fetch.bind(window) });
        return;
      } catch { /* sin servidor de la Pi: se usa el demo, que lo avisa con una franja */ }
    }
    const velocidad = Number(params.get("velocidad")) || 1;
    estado.demo = FuenteDemo.crear(modelo, { velocidad });
    estado.demo.iniciar();
    estado.comandos = estado.demo.comandos;
    // Para ensayos y capturas: ?falla=ATASCO (u otra) la provoca y ?previo=N adelanta N pasos.
    if (params.get("falla")) estado.demo.inyectar(params.get("falla"));
    for (let i = 0; i < (Number(params.get("previo")) || 0); i++) estado.demo.avanzar();
  }

  arrancar();
  window.Triar.app = { modelo, estado, pintar };   // para depurar desde la consola
})();
