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

    const batteryStatus = d.battery_status === 1 ? "Charging" : (d.battery_status === 0 ? "Battery" : "–");
    document.getElementById("battery_status").textContent = batteryStatus;
    document.getElementById("battery_level").textContent = formatValue(d.battery_level, "", 0);
    document.getElementById("runtime_hours").textContent = formatValue(d.runtime_hours, "", 0);
    document.getElementById("pc03").textContent = formatValue(d.pc03, "", 0);
    document.getElementById("pc05").textContent = formatValue(d.pc05, "", 0);
    document.getElementById("pc10").textContent = formatValue(d.pc10, "", 0);
    document.getElementById("pc25").textContent = formatValue(d.pc25, "", 0);
    document.getElementById("pc50").textContent = formatValue(d.pc50, "", 0);
    document.getElementById("pc100").textContent = formatValue(d.pc100, "", 0);
  } catch (e) {
    console.error("Sensor fetch exception:", e);
  }
}

setInterval(refreshSensor, 2000);
refreshSensor();
