/* Vistas: funciones puras que devuelven HTML (cadenas). Nada aquí toca el DOM.
   Todo texto que viene de los datos pasa por Formato.esc. */
(function (root) {
  const Triar = (root.Triar = root.Triar || {});
  const F = () => Triar.Formato;
  const esc = (v) => F().esc(v);

  /* ---------------------------------------------------------------- marca */
  const SIMBOLO =
    '<g fill="none" stroke="currentColor" stroke-width="20" stroke-linecap="round">' +
    '<circle cx="92" cy="128" r="24"/><path d="M22 128H58"/><path d="M124 112C152 98 160 44 230 44"/>' +
    '<path d="M132 128H192"/><path d="M124 144C152 158 160 212 230 212"/></g>';
  const NOMBRE =
    '<g transform="translate(290 -8)"><g fill="none" stroke="currentColor" stroke-width="18" stroke-linecap="round">' +
    '<path d="M10 70H90M50 70V200"/><path d="M130 110V200M130 152Q130 112 172 112"/><path d="M215 110V200"/>' +
    '<circle cx="275" cy="155" r="44"/><path d="M319 111V200"/><path d="M370 110V200M370 152Q370 112 412 112"/></g>' +
    '<circle cx="215" cy="70" r="11" fill="currentColor"/></g>';

  function logo(variante) {
    if (variante === "horizontal") {
      return `<svg class="logo logo-h" role="img" aria-label="Triar" viewBox="0 0 760 256" height="32">${SIMBOLO}${NOMBRE}</svg>`;
    }
    return `<svg class="logo logo-s" role="img" aria-label="Triar" viewBox="0 0 256 256" width="32" height="32">${SIMBOLO}</svg>`;
  }

  /* ---------------------------------------------------------- encabezado */
  function encabezado(modelo, { modo }) {
    const info = modelo.estado && F().ESTADOS[modelo.estado] ? F().ESTADOS[modelo.estado] : { tono: "neutro" };
    const enlace = modelo.enlaceOk ? "conectado" : "sin latido";
    const paro = modo === "operacion"
      ? '<div class="paro"><button type="button" class="btn btn-peligro" data-accion="paro">PARO</button>' +
        '<span class="leyenda-paro">Paro por software; el seguro es el botón físico</span></div>'
      : "";
    const demo = modelo.modo === "demo"
      ? '<div class="franja franja-demo"><strong>Modo demostración — datos inventados</strong></div>' : "";
    const cargando = modelo.modo === null
      ? '<div class="franja franja-carga" role="status">Conectando con la Pi…</div>' : "";
    const conexion = modelo.servidorOk === false
      ? '<div class="franja franja-error" role="alert">Sin conexión con la Pi; reintentando…</div>' : "";
    return `${demo}${cargando}${conexion}<header class="encabezado">` +
      `<div class="marca">${logo("horizontal")}${logo("simbolo")}<span class="subtitulo">Banda Clasificadora</span></div>` +
      '<div class="sistema">' +
      `<span class="chip" data-tono="${modelo.enlaceOk ? "ok" : "aviso"}"><span>Enlace con el ESP32: ${enlace}</span></span>` +
      `<span class="chip" data-tono="${esc(info.tono)}"><span>${esc(F().textoEstado(modelo.estado))}</span></span>` +
      `</div>${paro}</header>`;
  }

  /* ----------------------------------------------------------- pestañas */
  const ICONOS = {
    "tiempo-real": '<path d="M3 12h4l2-6 4 12 2-6h6"/>',
    inventario: '<path d="M4 8l8-4 8 4v8l-8 4-8-4zM4 8l8 4 8-4M12 12v8"/>',
    etiquetas: '<path d="M4 4h8l8 8-8 8-8-8zM8.5 8.5h.01"/>',
    historial: '<path d="M5 6h14M5 12h14M5 18h14"/>',
    alertas: '<path d="M12 4l9 16H3zM12 10v4M12 17h.01"/>',
  };
  const SECCIONES = [
    ["tiempo-real", "Tiempo real"], ["inventario", "Inventario"], ["etiquetas", "Etiquetas"],
    ["historial", "Historial"], ["alertas", "Alertas"],
  ];

  function pestanas(modo, { activa = "tiempo-real", alertas = 0 } = {}) {
    const botones = SECCIONES.filter(([id]) => id !== "etiquetas" || modo === "operacion").map(([id, texto]) => {
      const pendientes = id === "alertas" && alertas > 0
        ? `<span class="insignia" aria-label="${alertas} sin reconocer">${alertas}</span>` : "";
      return `<button type="button" class="pestana" data-vista="${id}"${id === activa ? ' aria-current="page"' : ""}>` +
        `<svg viewBox="0 0 24 24" width="24" height="24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">${ICONOS[id]}</svg>` +
        `<span>${texto}</span>${pendientes}</button>`;
    });
    return `<nav class="pestanas" aria-label="Secciones">${botones.join("")}</nav>`;
  }

  /* --------------------------------------------------------- tiempo real */
  const ETAPAS = {
    entrada: "Entrada", inspeccion: "Sensor de inspección", lector: "Lector NFC",
    salida: "Sensor de salida", compuerta: "Compuerta",
  };
  const CONTENEDORES = { A: "Contenedor A", B: "Contenedor B", R: "Rechazo" };
  const VENTANA_DESTINO_MS = 2500;

  function contenedor(modelo, cat, destino) {
    const c = modelo.contenedores[cat] || { contenido: 0, capacidad: 0 };
    const lleno = c.capacidad > 0 && c.contenido >= c.capacidad;
    const clases = ["contenedor", destino ? "destino" : "", lleno ? "lleno" : ""].filter(Boolean).join(" ");
    const cifra = c.capacidad > 0 ? `${esc(c.contenido)} de ${esc(c.capacidad)}` : `${esc(c.contenido)} piezas`;
    return `<li class="${clases}" data-cat="${cat}">` +
      `<span class="cat cat-${cat}">${cat}</span><span class="cont-nombre">${CONTENEDORES[cat]}</span>` +
      `<span class="cont-cifra">${cifra}${lleno ? " · <strong>Lleno</strong>" : ""}</span>` +
      `<span class="barra" style="--p:${modelo.porcentajeContenedor(cat)}%" aria-hidden="true"></span></li>`;
  }

  const AYUDA_ESTADO = {
    PAUSA: "Pulsa Restablecer en la banda para arrancar.",
    PAUSA_CONTENEDOR: "Vacía el contenedor lleno y márcalo como vaciado.",
    ERROR: "Restablece la banda con el botón físico.",
    EMERGENCIA: "Suelta el paro y restablece la banda con el botón físico.",
  };
  const MOTOR_ENCENDIDO = ["ESPERA", "LIBERA_INSPECCION", "CONFIRMA_SALIDA", "RETORNO"];

  function banda(modelo, { orientacion, ahora }) {
    const { etapa, tono } = Triar.Etapas.de(modelo.estado);
    const ultima = modelo.ultimaPieza();
    const destino = ultima && ahora - ultima.ts >= 0 && ahora - ultima.ts < VENTANA_DESTINO_MS ? ultima.categoria : null;
    const pasos = Triar.Etapas.ORDEN.map((id) => {
      const activa = id === etapa;
      return `<li class="etapa${activa ? " activa" : ""}" data-etapa="${id}" data-tono="${activa ? tono : "neutro"}"` +
        `${activa ? ' aria-current="step"' : ""}><span class="etapa-punto" aria-hidden="true"></span>` +
        `<span class="etapa-nombre">${ETAPAS[id]}</span></li>`;
    }).join("");
    const cont = ["A", "B", "R"].map((c) => contenedor(modelo, c, c === destino)).join("");
    return `<section class="banda ${orientacion === "vertical" ? "banda-v" : "banda-h"}" data-motor="${MOTOR_ENCENDIDO.includes(modelo.estado) ? "on" : "off"}" aria-label="Estado de la banda">` +
      `<p class="banda-estado" data-tono="${esc(tono)}"><span class="etiqueta-seccion">Estado</span> ` +
      `<strong>${esc(F().textoEstado(modelo.estado))}</strong>` +
      `${AYUDA_ESTADO[modelo.estado] ? ` <span class="secundario pequeno">${AYUDA_ESTADO[modelo.estado]}</span>` : ""}</p>` +
      `<div class="cinta-caja"><div class="cinta" aria-hidden="true"></div><ol class="etapas">${pasos}</ol></div>` +
      `<ul class="contenedores" aria-label="Contenedores">${cont}</ul></section>`;
  }

  const RECIENTES = 12;

  function recientes(modelo) {
    const ult = modelo.piezas.slice(-RECIENTES);
    if (!ult.length) return "";
    const items = ult.map((p, i) =>
      `<li class="cat cat-${esc(p.categoria)} reciente${i === ult.length - 1 ? " reciente-ultima" : ""}" title="${esc(p.nombre || "Sin etiqueta")}">${esc(p.categoria)}</li>`).join("");
    return `<ol class="recientes" aria-label="Últimas piezas, la más reciente a la derecha">${items}</ol>`;
  }

  function ultimaPieza(modelo, demo) {
    const p = modelo.ultimaPieza();
    if (!p) {
      return '<section class="tarjeta ultima ticket"><div class="ticket-cuerpo"><h3 class="etiqueta-seccion">Última pieza</h3>' +
        '<p class="secundario">Aún no hay piezas. ' +
        (demo ? "La demostración las generará en unos segundos." : "Coloca una pieza con etiqueta en la banda.") + "</p></div></section>";
    }
    const destino = p.categoria === "R" ? "Rechazo" : CONTENEDORES[p.categoria] || p.categoria;
    const motivo = p.motivo ? `<p class="secundario">${esc(F().etiquetaMotivo(p.motivo))}</p>` : "";
    return `<section class="tarjeta ultima ticket" data-cat="${esc(p.categoria)}"><div class="ticket-cuerpo"><h3 class="etiqueta-seccion">Última pieza</h3>` +
      `<p class="ultima-titulo">${esc(p.nombre || "Sin etiqueta")} → <span class="cat cat-${esc(p.categoria)}">${esc(p.categoria)}</span> ${esc(destino)}</p>${motivo}` +
      `<p class="mono secundario">${esc(F().uidCorto(p.uid))} · #${esc(p.seq)} · ${esc(F().hora(p.ts))} · ${esc(F().duracion(p.ciclo_ms))}</p>` +
      `${recientes(modelo)}</div></section>`;
  }

  const DESTINOS = [["A", "A"], ["B", "B"], ["R", "Rechazo"]];

  function resumen(modelo) {
    const c = modelo.conteos();
    const ciclos = modelo.ciclosRecientes();
    const promedio = ciclos.length ? F().duracion(ciclos.reduce((a, b) => a + b, 0) / ciclos.length) : "—";
    const tramos = DESTINOS.filter(([k]) => c[k] > 0)
      .map(([k]) => `<i class="tramo tramo-${k}" style="flex:${c[k]}"></i>`).join("");
    const aria = DESTINOS.map(([k, n]) => `${n} ${c[k]}`).join(", ");
    const leyenda = DESTINOS.map(([k, n]) => {
      const pct = c.total ? ` · ${Math.round((100 * c[k]) / c.total)} %` : "";
      return `<div class="ley ley-${k}"><dt>${n}${pct}</dt><dd class="cifra-valor">${esc(c[k])}</dd></div>`;
    }).join("");
    return '<section class="tarjeta resumen"><h3 class="etiqueta-seccion">Resumen</h3>' +
      `<div class="resumen-cab"><p class="resumen-total"><span class="cifra-valor">${esc(c.total)}</span> <span class="secundario">piezas</span></p>` +
      `<p class="secundario">Ciclo prom. <strong>${esc(promedio)}</strong></p></div>` +
      `<div class="reparto" role="img" aria-label="Reparto: ${esc(aria)}">${tramos}</div>` +
      `<dl class="leyenda-reparto">${leyenda}</dl></section>`;
  }

  function graficas(modelo, ahora) {
    const porMinuto = modelo.piezasPorMinuto(ahora, 10);
    const ciclos = modelo.ciclosRecientes(30);
    const ultimoCiclo = ciclos.length ? F().duracion(ciclos[ciclos.length - 1]) : "sin datos";
    return '<section class="tarjeta graficas">' +
      `<div><h3 class="etiqueta-seccion">Piezas por minuto</h3>${Triar.Graficas.barras(porMinuto, { ancho: 240, alto: 56, etiqueta: "Piezas por minuto, últimos 10 minutos" })}` +
      `<p class="pequeno secundario">Último minuto: ${esc(porMinuto[porMinuto.length - 1])}</p></div>` +
      `<div><h3 class="etiqueta-seccion">Tiempo de ciclo</h3>${Triar.Graficas.sparkline(ciclos.map((c) => c / 1000), { ancho: 240, alto: 56, etiqueta: "Tiempo de ciclo en segundos" })}` +
      `<p class="pequeno secundario">Último: ${esc(ultimoCiclo)}</p></div></section>`;
  }

  function feed(modelo, limite) {
    const items = modelo.feed.slice(0, limite).map((e) =>
      `<li class="evento" data-tono="${esc(e.tono)}"><time class="mono">${esc(F().hora(e.ts))}</time><span>${esc(e.texto)}</span></li>`).join("");
    return `<section class="tarjeta feed-caja"><h3 class="etiqueta-seccion">Eventos</h3>` +
      (items ? `<ol class="feed">${items}</ol>` : '<p class="secundario">Todavía no hay eventos.</p>') + "</section>";
  }

  function tiempoReal(modelo, { modo, ahora, demo }) {
    const op = modo === "operacion";
    return `<div class="tiempo-real">${banda(modelo, { orientacion: op ? "horizontal" : "vertical", ahora })}` +
      '<div class="rejilla-tr">' +
      `${ultimaPieza(modelo, demo)}${resumen(modelo)}` +
      `${op ? graficas(modelo, ahora) : ""}${feed(modelo, op ? 12 : 5)}</div></div>`;
  }

  /* ------------------------------------------------------------ inventario */
  const nombreObjeto = (p) => p.nombre || (p.uid ? F().uidCorto(p.uid) : "Sin etiqueta");
  const insigniaCat = (c) => `<span class="cat cat-${esc(c)}">${esc(c)}</span>`;
  const destinoCat = (c) => `<span class="destino-cat">${insigniaCat(c)}${c === "R" ? "Rechazo" : ""}</span>`;

  function porObjeto(modelo) {
    const mapa = new Map();
    for (const p of modelo.piezas) {
      const clave = p.uid || "";
      const e = mapa.get(clave) || { uid: p.uid, nombre: null, categoria: p.categoria, cantidad: 0, ultima: 0 };
      e.cantidad += 1;
      if (p.nombre) e.nombre = p.nombre;
      if (p.ts >= e.ultima) { e.ultima = p.ts; e.categoria = p.categoria; }
      mapa.set(clave, e);
    }
    return [...mapa.values()].sort((a, b) => b.cantidad - a.cantidad || b.ultima - a.ultima);
  }

  function tarjetaInventario(modelo, cat, op) {
    const c = modelo.contenedores[cat] || { contenido: 0, capacidad: 0 };
    const lleno = c.capacidad > 0 && c.contenido >= c.capacidad;
    const cifra = c.capacidad > 0 ? `${esc(c.contenido)} de ${esc(c.capacidad)}` : `${esc(c.contenido)} piezas`;
    const controles = op
      ? '<div class="controles">' +
        `<button type="button" class="btn" data-accion="vaciado" data-categoria="${cat}">Marcar vaciado</button>` +
        `<label class="solo-lectores" for="cap-${cat}">Capacidad del ${esc(CONTENEDORES[cat].toLowerCase())}</label>` +
        `<input id="cap-${cat}" class="campo campo-num" type="number" inputmode="numeric" min="1" max="999" value="${esc(c.capacidad)}" data-capacidad="${cat}">` +
        `<button type="button" class="btn" data-accion="capacidad" data-categoria="${cat}">Cambiar capacidad</button></div>`
      : "";
    return `<li class="tarjeta contenedor-inv${lleno ? " lleno" : ""}" data-cat="${cat}">` +
      `<div class="cab">${insigniaCat(cat)}<h3>${CONTENEDORES[cat]}</h3></div>` +
      `<p class="cifra-grande">${cifra}${lleno ? " · <strong>Lleno</strong>" : ""}</p>` +
      `<span class="barra" style="--p:${modelo.porcentajeContenedor(cat)}%" aria-hidden="true"></span>${controles}</li>`;
  }

  function inventario(modelo, { modo }) {
    const op = modo === "operacion";
    const filas = porObjeto(modelo).map((o) =>
      `<tr><td>${esc(o.nombre || (o.uid ? F().uidCorto(o.uid) : "Sin etiqueta"))}</td><td>${destinoCat(o.categoria)}</td>` +
      `<td class="num">${esc(o.cantidad)}</td><td class="mono">${esc(F().hora(o.ultima))}</td></tr>`).join("");
    return '<div class="inventario"><div class="vista-titulo"><h2>Inventario</h2></div>' +
      `<ul class="rejilla-inv">${["A", "B", "R"].map((c) => tarjetaInventario(modelo, c, op)).join("")}</ul>` +
      '<section class="tarjeta"><h3 class="etiqueta-seccion">Por objeto</h3>' +
      (filas
        ? `<div class="tabla-caja"><table class="tabla"><thead><tr><th>Objeto</th><th>Destino</th><th class="num">Cantidad</th><th>Última lectura</th></tr></thead><tbody>${filas}</tbody></table></div>`
        : '<p class="secundario">Sin piezas todavía.</p>') + "</section></div>";
  }

  /* ------------------------------------------------------------ historial */
  const LIMITE_HISTORIAL = { operacion: 200, supervision: 20 };

  function historial(modelo, { modo, filtro = "todas", demo }) {
    const op = modo === "operacion";
    const visibles = modelo.piezas.filter((p) => filtro === "todas" || p.categoria === filtro)
      .slice(-LIMITE_HISTORIAL[op ? "operacion" : "supervision"]).reverse();
    const origen = (p) => (p.tardio || p.reenvio ? "Reenviada" : "En vivo");
    let cuerpo;
    if (!visibles.length) {
      cuerpo = '<p class="secundario">Sin piezas todavía.</p>';
    } else if (op) {
      const filas = visibles.map((p) =>
        `<tr class="fila-pieza"><td class="mono">${esc(F().hora(p.ts))}</td><td class="num">#${esc(p.seq)}</td>` +
        `<td>${esc(nombreObjeto(p))}</td><td>${destinoCat(p.categoria)}</td><td>${esc(F().etiquetaMotivo(p.motivo))}</td>` +
        `<td class="num mono">${esc(F().duracion(p.ciclo_ms))}</td><td>${origen(p)}</td></tr>`).join("");
      cuerpo = `<div class="tabla-caja"><table class="tabla"><thead><tr><th>Hora</th><th class="num">N.º</th><th>Objeto</th><th>Destino</th><th>Motivo</th><th class="num">Ciclo</th><th>Origen</th></tr></thead><tbody>${filas}</tbody></table></div>`;
    } else {
      cuerpo = '<ol class="lista-piezas">' + visibles.map((p) =>
        `<li class="pieza-item"><div class="pieza-linea"><strong>#${esc(p.seq)} ${esc(nombreObjeto(p))}</strong>${destinoCat(p.categoria)}</div>` +
        `<p class="mono secundario pequeno">${esc(F().hora(p.ts))} · ${esc(F().duracion(p.ciclo_ms))}${p.motivo ? " · " + esc(F().etiquetaMotivo(p.motivo)) : ""}</p></li>`).join("") + "</ol>";
    }
    const opcion = (v, t) => `<option value="${v}"${filtro === v ? " selected" : ""}>${t}</option>`;
    const herramientas = op
      ? '<div class="herramientas"><label class="pequeno secundario" for="filtro-destino">Destino</label>' +
        `<select id="filtro-destino" class="campo" data-filtro aria-label="Filtrar por destino">${opcion("todas", "Todos")}${opcion("A", "A")}${opcion("B", "B")}${opcion("R", "Rechazo")}</select>` +
        (demo ? "" : '<a class="btn btn-enlace" href="api/historial.csv" download data-accion="exportar-csv">Exportar CSV</a>') + "</div>"
      : "";
    return `<div class="historial"><div class="vista-titulo"><h2>Historial</h2>${herramientas}</div><section class="tarjeta">${cuerpo}</section></div>`;
  }

  /* --------------------------------------------------------------- alertas */
  const NIVELES = { critico: ["Crítica", "critico"], advertencia: ["Advertencia", "aviso"], info: ["Info", "neutro"] };

  function alertas(modelo, { modo }) {
    const op = modo === "operacion";
    const lista = modelo.alertas.slice(0, 50).map((a) => {
      const [texto, tono] = NIVELES[a.nivel] || ["Info", "neutro"];
      return `<li class="alerta${a.reconocida ? "" : " nueva"}" data-nivel="${esc(a.nivel)}">` +
        `<span class="chip" data-tono="${tono}"><span>${texto}</span></span>` +
        `<div class="alerta-cuerpo"><p>${esc(a.mensaje)}</p><p class="pequeno secundario"><span class="mono">${esc(a.codigo)}</span> · ` +
        `<time class="mono">${esc(F().hora(a.ts))}</time>${a.reconocida ? "" : " · <strong>Nueva</strong>"}</p></div></li>`;
    }).join("");
    const reconocer = op
      ? `<button type="button" class="btn" data-accion="reconocer-alertas"${modelo.alertasSinReconocer() ? "" : " disabled"}>Reconocer todas</button>` : "";
    return `<div class="alertas"><div class="vista-titulo"><h2>Alertas</h2>${reconocer}</div><section class="tarjeta">` +
      (lista ? `<ul class="lista-alertas">${lista}</ul>` : '<p class="secundario">Sin alertas. Todo en orden.</p>') + "</section></div>";
  }

  /* -------------------------------------------------------------- etiquetas */
  function etiquetas(modelo, { modo }) {
    if (modo !== "operacion") return "";
    const desactualizada = modelo.versionTabla !== null && modelo.versionEsp32 !== null && modelo.versionTabla !== modelo.versionEsp32;
    const aviso = desactualizada
      ? `<p class="franja" role="status">La tabla del ESP32 está desactualizada (v${esc(modelo.versionEsp32)} de v${esc(modelo.versionTabla)}). Pulsa Sincronizar con ESP32.</p>` : "";
    const sinRegistrar = modelo.uidsSinRegistrar();
    const detectados = sinRegistrar.length
      ? '<section class="tarjeta"><h3 class="etiqueta-seccion">Etiquetas detectadas sin registrar</h3><ul class="lista-uid">' +
        sinRegistrar.map((u) => `<li><code>${esc(u)}</code> <button type="button" class="btn" data-rellenar-uid="${esc(u)}">Registrar</button></li>`).join("") + "</ul></section>"
      : "";
    const filas = modelo.etiquetas.map((e) =>
      `<tr><td class="mono">${esc(e.uid)}</td><td>${esc(e.nombre)}</td><td>${destinoCat(e.categoria)}</td>` +
      `<td class="num"><button type="button" class="btn" data-accion="eliminar-etiqueta" data-uid="${esc(e.uid)}" data-nombre="${esc(e.nombre)}">Eliminar</button></td></tr>`).join("");
    const tabla = filas
      ? `<div class="tabla-caja"><table class="tabla"><thead><tr><th>UID</th><th>Nombre</th><th>Categoría</th><th></th></tr></thead><tbody>${filas}</tbody></table></div>`
      : '<p class="secundario">Aún no hay etiquetas. Registra la primera arriba.</p>';
    return '<div class="etiquetas"><div class="vista-titulo"><h2>Etiquetas</h2>' +
      '<button type="button" class="btn btn-primario" data-accion="sincronizar">Sincronizar con ESP32</button></div>' + aviso +
      '<section class="tarjeta"><h3 class="etiqueta-seccion">Registrar etiqueta</h3>' +
      '<form class="formulario" data-accion="registrar-etiqueta" autocomplete="off">' +
      '<label class="campo-grupo"><span>UID</span><input id="campo-uid" name="uid" class="campo mono" required placeholder="04A31FB26C8000" autocapitalize="characters" spellcheck="false"></label>' +
      '<label class="campo-grupo"><span>Nombre</span><input id="campo-nombre" name="nombre" class="campo" required maxlength="60" placeholder="Llave"></label>' +
      '<label class="campo-grupo"><span>Categoría</span><select id="campo-categoria" name="categoria" class="campo">' +
      '<option value="A">A</option><option value="B">B</option><option value="R">R · Rechazo</option></select></label>' +
      '<button type="submit" class="btn btn-primario">Registrar etiqueta</button></form></section>' +
      `${detectados}<section class="tarjeta"><h3 class="etiqueta-seccion">Tabla de etiquetas</h3>${tabla}</section></div>`;
  }

  Triar.Vistas = Object.assign(Triar.Vistas || {}, { logo, encabezado, pestanas, banda, tiempoReal, inventario, historial, alertas, etiquetas });
  if (typeof module !== "undefined") module.exports = Triar.Vistas;
})(typeof window !== "undefined" ? window : globalThis);
