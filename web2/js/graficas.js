/* Gráficas mínimas en SVG, sin librerías. Devuelven HTML (cadena). */
(function (root) {
  const Triar = (root.Triar = root.Triar || {});

  const num = (n) => Number(n.toFixed(1));
  const esc = (t) => String(t).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));

  function sparkline(valores, { ancho, alto, etiqueta = "Gráfica" }) {
    const v = valores.length ? valores : [0, 0];
    const lista = v.length === 1 ? [v[0], v[0]] : v;
    const max = Math.max(...lista, 0);
    const puntos = lista.map((valor, i) => {
      const x = lista.length === 1 ? 0 : (i * ancho) / (lista.length - 1);
      const y = max > 0 ? alto - 1 - (valor / max) * (alto - 2) : alto - 1;
      return `${num(x)},${num(y)}`;
    });
    const resumen = valores.length ? `, último valor ${valores[valores.length - 1]}` : ", sin datos";
    return `<svg role="img" aria-label="${esc(etiqueta)}${esc(resumen)}" viewBox="0 0 ${ancho} ${alto}" width="${ancho}" height="${alto}" preserveAspectRatio="none">` +
      `<polyline points="${puntos.join(" ")}" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round" stroke-linecap="round"/></svg>`;
  }

  function barras(valores, { ancho, alto, etiqueta = "Gráfica" }) {
    const n = valores.length;
    const max = Math.max(...valores, 0);
    const hueco = 2;
    const w = n ? Math.max(1, ancho / n - hueco) : 0;
    const rects = valores.map((valor, i) => {
      const h = valor > 0 && max > 0 ? Math.max(1, (valor / max) * alto) : 0;
      return `<rect x="${num(i * (ancho / n))}" y="${num(alto - h)}" width="${num(w)}" height="${num(h)}" rx="1" fill="currentColor"/>`;
    });
    return `<svg role="img" aria-label="${esc(etiqueta)}" viewBox="0 0 ${ancho} ${alto}" width="${ancho}" height="${alto}" preserveAspectRatio="none">${rects.join("")}</svg>`;
  }

  Triar.Graficas = { sparkline, barras };
  if (typeof module !== "undefined") module.exports = Triar.Graficas;
})(typeof window !== "undefined" ? window : globalThis);
