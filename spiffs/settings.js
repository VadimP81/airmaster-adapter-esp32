async function loadSettings() {
  try {
    const r = await fetch("/api/settings");
    const s = await r.json();

    document.getElementById("version").textContent = "v" + s.version;
    document.getElementById("ssid").value = s.wifi.ssid || "";
    document.getElementById("password").value = "";
    document.getElementById("broker").value = s.mqtt.broker || "";
    document.getElementById("port").value = s.mqtt.port || 1883;
    document.getElementById("user").value = s.mqtt.user || "";
    document.getElementById("pass").value = "";
    document.getElementById("interval").value = s.interval || 10;

  } catch(e) { alert("Failed to load settings"); console.log(e); }
}

async function saveSettings(event) {
  event.preventDefault();
  const btn = document.querySelector("button[type='submit']");
  btn.disabled = true; btn.textContent = "Saving...";

  const data = {
    wifi: { ssid: document.getElementById("ssid").value, pass: document.getElementById("password").value },
    mqtt: {
      broker: document.getElementById("broker").value,
      port: parseInt(document.getElementById("port").value),
      user: document.getElementById("user").value,
      pass: document.getElementById("pass").value
    },
    interval: parseInt(document.getElementById("interval").value)
  };

  try {
    const r = await fetch("/api/settings", {
      method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify(data)
    });
    const resp = await r.json();
    if(resp.ok) {
      if(confirm("Settings saved. Reboot now to apply changes?")) {
        fetch("/api/reboot",{method:"POST"});
        document.body.innerHTML="<h2>Rebooting...</h2>";
      }
    } else { alert("Failed to save settings"); btn.disabled=false; btn.textContent="Save & Reboot"; }
  } catch(e) { alert("Error saving settings"); console.log(e); btn.disabled=false; btn.textContent="Save & Reboot"; }
}

document.getElementById("settingsForm").addEventListener("submit", saveSettings);
window.onload = loadSettings;
