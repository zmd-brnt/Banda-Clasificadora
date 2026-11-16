/* Modo de pantalla: ancha = supervisión + controles; angosta = solo supervisión. */
(function (root) {
  const Triar = (root.Triar = root.Triar || {});

  const UMBRAL_PX = 900;
  const ACCIONES = ["paro", "vaciado", "capacidad", "sincronizar", "registrar-etiqueta",
    "eliminar-etiqueta", "reconocer-alertas", "exportar-csv"];

  function deAncho(px) {
    return px >= UMBRAL_PX ? "operacion" : "supervision";
  }

  function puede(modo, accion) {
    return modo === "operacion" && ACCIONES.includes(accion);
  }

  Triar.Modo = { UMBRAL_PX, ACCIONES, deAncho, puede };
  if (typeof module !== "undefined") module.exports = Triar.Modo;
})(typeof window !== "undefined" ? window : globalThis);
