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
    setStatus("sensor_status", s.connected, s.connected ? "Online" : "Offline");
    document.getElementById("last_update").textContent = (s.last_rx_sec || 0) + " sec ago";

    const d = s.data || {};
    document.getElementById("pm25").textContent = formatValue(d.pm25, " µg/m³");
    document.getElementById("pm10").textContent = formatValue(d.pm10, " µg/m³");
    document.getElementById("co2").textContent = formatValue(d.co2, " ppm");
    document.getElementById("tvoc").textContent = formatValue(d.tvoc, " mg/m³", 2);
    document.getElementById("hcho").textContent = formatValue(d.hcho, " mg/m³", 3);
    document.getElementById("temp").textContent = formatValue(d.temp, " °C", 1);
    document.getElementById("humidity").textContent = formatValue(d.humidity, " %", 1);
  } catch (e) {
    console.error("Sensor fetch exception:", e);
  }
}

setInterval(refreshSensor, 2000);
refreshSensor();
