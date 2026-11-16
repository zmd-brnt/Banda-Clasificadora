/* Traduce el estado del ESP32 a la etapa de la banda que se ilumina.
   Orden según docs/maquina-de-estados.md §4. Es el ESTADO, no la posición real de la pieza. */
(function (root) {
  const Triar = (root.Triar = root.Triar || {});

  const ORDEN = ["entrada", "inspeccion", "lector", "salida", "compuerta"];
  const ETAPA_DE_ESTADO = {
    ESPERA: "entrada", LIBERA_INSPECCION: "inspeccion", LECTURA_NFC: "lector",
    CONFIRMA_SALIDA: "salida", CLASIFICANDO: "compuerta", RETORNO: "compuerta",
  };

  function de(estado) {
    const info = estado && Triar.Formato ? Triar.Formato.ESTADOS[estado] : null;
    return { etapa: ETAPA_DE_ESTADO[estado] || null, tono: info ? info.tono : "neutro" };
  }

  Triar.Etapas = { ORDEN, de };
  if (typeof module !== "undefined") module.exports = Triar.Etapas;
})(typeof window !== "undefined" ? window : globalThis);
