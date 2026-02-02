function setStatus(id, ok, text) {
  const el = document.getElementById(id);
  el.className = "status " + (ok ? "ok" : "err");
  el.textContent = text;
}

function formatValue(value, suffix = "", decimals = null) {
  if (value === null || value === undefined || Number.isNaN(value)) {
    return "–";
  }
  if (decimals !== null && typeof value === "number") {
    return value.toFixed(decimals) + suffix;
  }
  return value + suffix;
}

async function refreshSensor() {
  try {
    const r = await fetch("/api/sensor");
    if (!r.ok) {
      console.error("Sensor fetch failed:", r.status);
      return;
    }
    const s = await r.json();

    document.getElementById("version").textContent = "v" + (s.version || "?");
    document.getElementById("last_update").textContent = (s.last_rx_sec || 0) + " sec ago";

    const d = s.data || {};
    document.getElementById("pm25").textContent = formatValue(d.pm25, "", 1);
    document.getElementById("pm10").textContent = formatValue(d.pm10, "", 1);
    document.getElementById("co2").textContent = formatValue(d.co2, "", 0);
    document.getElementById("tvoc").textContent = formatValue(d.tvoc, "", 2);
    document.getElementById("hcho").textContent = formatValue(d.hcho, "", 2);
    document.getElementById("temp").textContent = formatValue(d.temp, "", 2);
    document.getElementById("humidity").textContent = formatValue(d.humidity, "", 2);
  } catch (e) {
    console.error("Sensor fetch exception:", e);
  }
}

setInterval(refreshSensor, 2000);
refreshSensor();
