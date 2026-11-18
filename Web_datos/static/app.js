/* Helpers compartidos de las paginas de REDUX. */

async function obtenerJSON(url) {
  const resp = await fetch(url, { headers: { Accept: "application/json" } });
  if (resp.status === 401) {
    window.location.href = "/ingreso";
    return null;
  }
  const datos = await resp.json().catch(() => ({}));
  if (!resp.ok) {
    throw new Error(datos.error || "Error " + resp.status);
  }
  return datos;
}

async function enviarJSON(url, metodo, cuerpo) {
  const resp = await fetch(url, {
    method: metodo,
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(cuerpo),
  });
  const datos = await resp.json().catch(() => ({}));
  if (!resp.ok) {
    throw new Error(datos.error || "Error " + resp.status);
  }
  return datos;
}

function formatearFecha(iso) {
  if (!iso) return "-";
  const d = new Date(iso);
  if (isNaN(d)) return iso;
  return d.toLocaleString("es-AR");
}

/* Colores de las series del grafico. */
const COLOR_RANGO = "#0dcaf0"; /* celeste */
const COLOR_ASIST = "#d6b4ff"; /* violeta claro */
