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

const AIR_QUALITY_THRESHOLDS = {
  pm25: { warn: 35, bad: 55, unit: "µg/m³" },
  pm10: { warn: 50, bad: 100, unit: "µg/m³" },
  co2: { warn: 1000, bad: 1500, unit: "ppm" },
  tvoc: { warn: 0.3, bad: 0.6, unit: "mg/m³" },
  hcho: { warn: 0.1, bad: 0.3, unit: "mg/m³" }
};

function updateAirQualityStatus(key, value) {
  const item = document.getElementById(`aq-${key}`);
  const threshold = AIR_QUALITY_THRESHOLDS[key];
  if (!item || !threshold) return;

  item.classList.remove("aq-warn", "aq-bad");
  if (value === null || value === undefined || Number.isNaN(value)) {
    item.removeAttribute("title");
    return;
  }

  item.setAttribute(
    "title",
    `Yellow ≥ ${threshold.warn} ${threshold.unit}, Red ≥ ${threshold.bad} ${threshold.unit}`
  );

  if (value >= threshold.bad) {
    item.classList.add("aq-bad");
  } else if (value >= threshold.warn) {
    item.classList.add("aq-warn");
  }
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
    updateAirQualityStatus("pm25", d.pm25);
    updateAirQualityStatus("pm10", d.pm10);
    updateAirQualityStatus("co2", d.co2);
    updateAirQualityStatus("tvoc", d.tvoc);
    updateAirQualityStatus("hcho", d.hcho);
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
