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

function drawGauge(svgId, value, max, warn, bad) {
  const svg = document.getElementById(svgId);
  if (!svg) return;
  
  svg.innerHTML = '';
  svg.setAttribute('viewBox', '0 0 120 60');
  
  if (value === null || value === undefined || Number.isNaN(value)) {
    const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
    text.setAttribute('x', '60');
    text.setAttribute('y', '30');
    text.setAttribute('text-anchor', 'middle');
    text.setAttribute('dy', '0.3em');
    text.setAttribute('font-size', '16');
    text.setAttribute('fill', '#999');
    text.textContent = '–';
    svg.appendChild(text);
    return;
  }
  
  const radius = 45;
  const cx = 60, cy = 55;
  
  function polarToCartesian(angle, r = radius) {
    const rad = Math.PI - (angle * Math.PI) / 180;
    return {
      x: cx + r * Math.cos(rad),
      y: cy - r * Math.sin(rad)
    };
  }
  
  // Draw colored arc zones as strokes
  const warnAngle = 180 * (warn / max);
  const badAngle = 180 * (bad / max);
  const strokeWidth = 13;
  const arcRadius = radius - strokeWidth / 2;
  
  const colors = [
    { start: 0, end: warnAngle, color: '#4CAF50' },
    { start: warnAngle, end: badAngle, color: '#ff9800' },
    { start: badAngle, end: 180, color: '#f44336' }
  ];

  const trackStart = polarToCartesian(0, arcRadius);
  const trackEnd = polarToCartesian(180, arcRadius);
  const track = document.createElementNS('http://www.w3.org/2000/svg', 'path');
  track.setAttribute(
    'd',
    `M ${trackStart.x},${trackStart.y} A ${arcRadius},${arcRadius} 0 0,1 ${trackEnd.x},${trackEnd.y}`
  );
  track.setAttribute('fill', 'none');
  track.setAttribute('stroke', '#e6e6e6');
  track.setAttribute('stroke-width', strokeWidth);
  track.setAttribute('stroke-linecap', 'round');
  svg.appendChild(track);
  
  colors.forEach(zone => {
    if (zone.start < zone.end) {
      const p1 = polarToCartesian(zone.start, arcRadius);
      const p2 = polarToCartesian(zone.end, arcRadius);
      const largeArc = (zone.end - zone.start) > 90 ? 1 : 0;
      
      const arc = document.createElementNS('http://www.w3.org/2000/svg', 'path');
      arc.setAttribute(
        'd',
        `M ${p1.x},${p1.y} A ${arcRadius},${arcRadius} 0 ${largeArc},1 ${p2.x},${p2.y}`
      );
      arc.setAttribute('fill', 'none');
      arc.setAttribute('stroke', zone.color);
      arc.setAttribute('stroke-width', strokeWidth);
      arc.setAttribute('stroke-linecap', 'butt');
      svg.appendChild(arc);
    }
  });
  
  // Draw needle
  const valueAngle = 180 * (Math.min(value, max) / max);
  const needleEnd = polarToCartesian(valueAngle);
  
  const needle = document.createElementNS('http://www.w3.org/2000/svg', 'line');
  needle.setAttribute('x1', cx);
  needle.setAttribute('y1', cy);
  needle.setAttribute('x2', needleEnd.x);
  needle.setAttribute('y2', needleEnd.y);
  needle.setAttribute('stroke', '#2c3e50');
  needle.setAttribute('stroke-width', '3');
  needle.setAttribute('stroke-linecap', 'round');
  svg.appendChild(needle);
  
  // Center point
  const centerDot = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
  centerDot.setAttribute('cx', cx);
  centerDot.setAttribute('cy', cy);
  centerDot.setAttribute('r', '3');
  centerDot.setAttribute('fill', '#333');
  svg.appendChild(centerDot);
}

function getMockSensorPayload() {
  const now = Date.now();
  const t = (now / 1000) % 60;
  const wave = (base, amp, period) => base + amp * Math.sin((2 * Math.PI * t) / period);

  return {
    version: "mock",
    last_rx_sec: 1,
    data: {
      pm25: Number(wave(12, 8, 20).toFixed(1)),
      pm10: Number(wave(20, 12, 24).toFixed(1)),
      co2: Math.round(wave(650, 220, 30)),
      tvoc: Number(wave(0.18, 0.08, 22).toFixed(2)),
      hcho: Number(wave(0.06, 0.04, 18).toFixed(2)),
      temp: Number(wave(23.5, 1.2, 26).toFixed(1)),
      humidity: Number(wave(42, 6, 28).toFixed(1)),
      battery_status: 0,
      battery_level: 4,
      pc03: Math.round(wave(520, 120, 25)),
      pc05: Math.round(wave(300, 80, 23)),
      pc10: Math.round(wave(180, 60, 21)),
      pc25: Math.round(wave(90, 35, 19)),
      pc50: Math.round(wave(40, 18, 17)),
      pc100: Math.round(wave(12, 6, 15))
    }
  };
}

async function refreshSensor() {
  try {
    let s;
    if (location.protocol === "file:") {
      s = getMockSensorPayload();
    } else {
      const r = await fetch("/api/sensor");
      if (!r.ok) {
        console.error("Sensor fetch failed:", r.status);
        return;
      }
      s = await r.json();
    }

    document.getElementById("version").textContent = "v" + (s.version || "?");
    document.getElementById("last_update").textContent = (s.last_rx_sec || 0) + " sec ago";

    const d = s.data || {};
    document.getElementById("pm25").textContent = formatValue(d.pm25, "", 0);
    document.getElementById("pm10").textContent = formatValue(d.pm10, "", 0);
    document.getElementById("co2").textContent = formatValue(d.co2, "", 0);
    document.getElementById("tvoc").textContent = formatValue(d.tvoc, "", 2);
    document.getElementById("hcho").textContent = formatValue(d.hcho, "", 2);
    updateAirQualityStatus("pm25", d.pm25);
    updateAirQualityStatus("pm10", d.pm10);
    updateAirQualityStatus("co2", d.co2);
    updateAirQualityStatus("tvoc", d.tvoc);
    updateAirQualityStatus("hcho", d.hcho);
    drawGauge("gauge-pm25", d.pm25, 100, 35, 55);
    drawGauge("gauge-pm10", d.pm10, 150, 50, 100);
    drawGauge("gauge-co2", d.co2, 2000, 1000, 1500);
    drawGauge("gauge-tvoc", d.tvoc, 1.0, 0.3, 0.6);
    drawGauge("gauge-hcho", d.hcho, 0.5, 0.1, 0.3);
    document.getElementById("temp").textContent = formatValue(d.temp, "", 1);
    document.getElementById("humidity").textContent = formatValue(d.humidity, "", 1);

    const batteryStatus = d.battery_status === 1 ? "Charging" : (d.battery_status === 0 ? "Battery" : "–");
    document.getElementById("battery_status").textContent = batteryStatus;
    document.getElementById("battery_level").textContent = formatValue(d.battery_level, "", 0);
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
