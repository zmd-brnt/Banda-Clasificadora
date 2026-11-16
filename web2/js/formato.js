/* Utilidades de formato: texto seguro, horas, duraciones y nombres de estados. */
(function (root) {
  const Triar = (root.Triar = root.Triar || {});

  const ESCAPES = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" };

  function esc(valor) {
    if (valor === null || valor === undefined) return "";
    return String(valor).replace(/[&<>"']/g, (c) => ESCAPES[c]);
  }

  const dos = (n) => String(n).padStart(2, "0");

  function hora(ts) {
    const d = new Date(ts);
    return `${dos(d.getHours())}:${dos(d.getMinutes())}:${dos(d.getSeconds())}`;
  }

  function duracion(ms) {
    if (ms === null || ms === undefined || Number.isNaN(ms)) return "—";
    return `${(ms / 1000).toFixed(1)} s`;
  }

  function uidCorto(uid) {
    if (!uid) return "Sin etiqueta";
    return uid.length > 6 ? `${uid.slice(0, 4)}…${uid.slice(-2)}` : uid;
  }

  const ESTADOS = {
    PAUSA: { texto: "En pausa", tono: "aviso" },
    ESPERA: { texto: "Esperando pieza", tono: "ok" },
    LECTURA_NFC: { texto: "Leyendo etiqueta", tono: "lectura" },
    CLASIFICANDO: { texto: "Clasificando", tono: "ok" },
    LIBERA_INSPECCION: { texto: "Liberando inspección", tono: "ok" },
    CONFIRMA_SALIDA: { texto: "Confirmando salida", tono: "ok" },
    RETORNO: { texto: "Compuerta al centro", tono: "ok" },
    PAUSA_CONTENEDOR: { texto: "Contenedor lleno", tono: "aviso" },
    ERROR: { texto: "Error", tono: "critico" },
    EMERGENCIA: { texto: "Emergencia", tono: "critico" },
  };

  function textoEstado(estado) {
    if (estado === null || estado === undefined) return "Sin datos";
    return ESTADOS[estado] ? ESTADOS[estado].texto : String(estado);
  }

  function etiquetaMotivo(motivo) {
    if (motivo === "SIN_LECTURA") return "Sin lectura de etiqueta";
    if (motivo === "NO_REGISTRADA") return "Etiqueta no registrada";
    return "";
  }

  Triar.Formato = { esc, hora, duracion, uidCorto, ESTADOS, textoEstado, etiquetaMotivo };
  if (typeof module !== "undefined") module.exports = Triar.Formato;
})(typeof window !== "undefined" ? window : globalThis);
