/* Fuente real: lee la instantánea y los eventos de la Pi y envía los comandos del panel. */
(function (root) {
  const Triar = (root.Triar = root.Triar || {});

  async function conectar(modelo, deps) {
    const base = deps.base || "";
    const pedirInstantanea = async () => {
      const r = await deps.fetch(base + "api/estado");
      if (!r.ok) throw new Error(`La Pi respondió ${r.status}`);
      return r.json();
    };

    const inst = await pedirInstantanea();
    modelo.modo = "real";
    modelo.aplicarInstantanea(inst);

    const flujo = new deps.EventSource(base + "api/eventos");
    let cayo = false;
    flujo.onmessage = (e) => {
      try { modelo.aplicar(JSON.parse(e.data)); } catch { /* mensaje dañado: se ignora */ }
    };
    flujo.onerror = () => {
      cayo = true;
      modelo.aplicar({ tipo: "SERVIDOR", ok: false });
    };
    flujo.onopen = async () => {
      if (cayo) {
        // Durante la caída pudieron perderse eventos: se vuelve a sincronizar todo.
        try { modelo.aplicarInstantanea(await pedirInstantanea()); } catch { /* el próximo evento lo resuelve */ }
        cayo = false;
      }
      modelo.aplicar({ tipo: "SERVIDOR", ok: true });
    };
    return { cerrar: () => flujo.close() };
  }

  function comandos(deps) {
    const base = deps.base || "";
    async function api(metodo, ruta, cuerpo) {
      let r;
      try {
        r = await deps.fetch(base + ruta, {
          method: metodo,
          headers: { "Content-Type": "application/json" },
          body: cuerpo === undefined ? undefined : JSON.stringify(cuerpo),
        });
      } catch {
        throw new Error("Sin conexión con la Pi; inténtalo de nuevo.");
      }
      let datos = {};
      try { datos = await r.json(); } catch { /* respuesta sin cuerpo */ }
      if (!r.ok) throw new Error(datos.error || datos.mensaje || `Error ${r.status}`);
      return datos;
    }
    const comando = (cuerpo) => api("POST", "api/comando", cuerpo);
    return {
      paro: () => comando({ cmd: "PARO" }),
      vaciado: (categoria) => comando({ cmd: "VACIADO", categoria }),
      capacidad: (categoria, capacidad) => comando({ cmd: "CAPACIDAD", categoria, capacidad }),
      sincronizar: () => comando({ cmd: "SYNC" }),
      registrarEtiqueta: (uid, nombre, categoria) => api("POST", "api/etiquetas", { uid, nombre, categoria }),
      eliminarEtiqueta: (uid) => api("DELETE", "api/etiquetas/" + encodeURIComponent(uid)),
      reconocerAlertas: () => api("POST", "api/alertas/reconocer", {}),
    };
  }

  Triar.FuenteReal = { conectar, comandos };
  if (typeof module !== "undefined") module.exports = Triar.FuenteReal;
})(typeof window !== "undefined" ? window : globalThis);
