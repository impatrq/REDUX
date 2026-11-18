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

/* Muestra en hora de Argentina (24 h): dd/mm/aaaa hh:mm.
   Se guarda en UTC; el ISO del servidor trae offset +00:00. */
const ZONA_AR = "America/Argentina/Buenos_Aires";

function formatearFecha(iso) {
  if (!iso) return "-";
  const d = new Date(iso);
  if (isNaN(d)) return iso;
  const fmt = new Intl.DateTimeFormat("es-AR", {
    timeZone: ZONA_AR,
    day: "2-digit",
    month: "2-digit",
    year: "numeric",
    hour: "2-digit",
    minute: "2-digit",
    hour12: false,
  });
  const p = Object.fromEntries(fmt.formatToParts(d).map((x) => [x.type, x.value]));
  let hora = p.hour === "24" ? "00" : p.hour; /* algunos motores dan 24 a medianoche */
  return `${p.day}/${p.month}/${p.year} ${hora}:${p.minute}`;
}

/* Colores de las series del grafico. */
const COLOR_RANGO = "#0dcaf0"; /* celeste */
const COLOR_ASIST = "#d6b4ff"; /* violeta claro */
