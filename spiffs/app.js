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
    if (!r.ok) {
      console.error("Status fetch failed:", r.status);
      return;
    }
    const s = await r.json();

    document.getElementById("version").textContent = "v" + (s.version || "?");
    setStatus("am7", s.am7?.connected, s.am7?.connected ? "Online" : "Offline");
    setStatus("mqtt", s.mqtt?.connected, s.mqtt?.connected ? "Connected" : "Disconnected");

    document.getElementById("last").textContent = (s.am7?.last_rx_sec || 0) + " sec ago";
    document.getElementById("interval").textContent = (s.interval || 0) + " sec";
    document.getElementById("uptime").textContent = formatUptime(s.uptime || 0);

  } catch(e) { 
    console.error("Status fetch exception:", e);
  }
}

function rebootDevice() {
  if (confirm('Are you sure you want to reboot the device?')) {
    fetch('/api/reboot', { method: 'POST' })
      .then(() => {
        alert('Device is rebooting... Please wait about 10 seconds.');
      })
      .catch(error => console.error('Error rebooting device:', error));
  }
}

setInterval(refresh, 2000);
refresh();
