function setStatus(id, ok, text) {
  const el = document.getElementById(id);
  el.className = "status " + (ok ? "ok" : "err");
  el.textContent = text;
}

function formatUptime(sec) {
  const h = Math.floor(sec/3600), m = Math.floor((sec%3600)/60), s = sec%60;
  return `${h.toString().padStart(2,'0')}:${m.toString().padStart(2,'0')}:${s.toString().padStart(2,'0')}`;
}

async function refresh() {
  try {
    const r = await fetch("/api/status");
    const s = await r.json();

    document.getElementById("version").textContent = "v" + s.version;
    setStatus("am7", s.am7.connected, s.am7.connected ? "Online" : "Offline");
    setStatus("mqtt", s.mqtt.connected, s.mqtt.connected ? "Connected" : "Disconnected");
    setStatus("zigbee", s.zigbee.joined, s.zigbee.joined ? "Joined" : "Not joined");

    document.getElementById("last").textContent = s.am7.last_rx_sec + " sec ago";
    document.getElementById("interval").textContent = s.interval + " sec";
    document.getElementById("uptime").textContent = formatUptime(s.uptime);

  } catch(e) { console.log("Status fetch failed"); }
}

setInterval(refresh, 2000);
refresh();
