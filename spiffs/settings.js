async function loadSettings() {
  try {
    const r = await fetch("/api/settings");
    if (!r.ok) {
      showError("Failed to load settings (HTTP " + r.status + ")");
      return;
    }
    const s = await r.json();

    document.getElementById("version").textContent = "v" + (s.version || "?");
    document.getElementById("ssid").value = s.wifi?.ssid || "";
    document.getElementById("password").value = "";
    document.getElementById("broker").value = s.mqtt?.broker || "";
    document.getElementById("port").value = s.mqtt?.port || 1883;
    document.getElementById("user").value = s.mqtt?.user || "";
    document.getElementById("pass").value = "";
    document.getElementById("topic").value = s.mqtt?.topic || "airmaster/sensors";
    document.getElementById("device_name").value = s.device_name || "AirMaster Gateway";
    document.getElementById("interval").value = s.interval || 10;
    document.getElementById("ha_discovery").checked = s.ha_discovery !== false;

  } catch(e) { 
    showError("Failed to load settings: " + e.message);
  }
}

async function saveSettings(event) {
  event.preventDefault();
  const btn = document.querySelector("button[type='submit']");
  btn.disabled = true; btn.textContent = "Saving...";

  const data = {
    wifi: { 
      ssid: document.getElementById("ssid").value, 
      pass: document.getElementById("password").value 
    },
    mqtt: {
      broker: document.getElementById("broker").value,
      port: parseInt(document.getElementById("port").value),
      user: document.getElementById("user").value,
      pass: document.getElementById("pass").value,
      topic: document.getElementById("topic").value
    },
    device_name: document.getElementById("device_name").value,
    interval: parseInt(document.getElementById("interval").value),
    ha_discovery: document.getElementById("ha_discovery").checked
  };

  try {
    const r = await fetch("/api/settings", {
      method:"POST", 
      headers:{"Content-Type":"application/json"}, 
      body:JSON.stringify(data)
    });
    
    if (!r.ok) {
      showError("Save failed (HTTP " + r.status + ")");
      btn.disabled=false; 
      btn.textContent="Save & Reboot";
      return;
    }
    
    const resp = await r.json();
    if(resp.ok) {
      if(confirm("Settings saved. Reboot now to apply changes?")) {
        fetch("/api/reboot",{method:"POST"});
        document.body.innerHTML="<div class='card'><h2>Rebooting...</h2><p>Please wait 10 seconds.</p></div>";
      } else {
        btn.disabled=false; 
        btn.textContent="Save & Reboot";
      }
    } else { 
      showError(resp.error || "Failed to save settings");
      btn.disabled=false; 
      btn.textContent="Save & Reboot"; 
    }
  } catch(e) { 
    showError("Error: " + e.message);
    btn.disabled=false; 
    btn.textContent="Save & Reboot"; 
  }
}

function showError(msg) {
  alert("⚠️ " + msg);
  console.error(msg);
}

document.getElementById("settingsForm").addEventListener("submit", saveSettings);
window.onload = loadSettings;
