#include "web_pages.h"
// ===================================================================================================================================================
const char MAIN_PAGE_HEAD[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>Printer State LED</title>
<style>
  body { font-family: Arial, sans-serif; font-size: 14px; }
  label { display:inline-block; width:150px; }
</style>
</head>
<body>
)rawliteral";

// ===================================================================================================================================================
const char MAIN_PAGE_STATE[] PROGMEM = R"rawliteral(
<script>
async function updatePrinterStatus(){
    try {
        const r = await fetch('/status');
        if (!r.ok) return;
        const d = await r.json();
        document.getElementById('state').innerText = d.state;
        document.getElementById('progress').innerText = (d.progress*100).toFixed(1) + ' %';
        document.getElementById('nozzle').innerText = d.nozzle.toFixed(1) + ' °C';
        document.getElementById('bed').innerText = d.bed.toFixed(1) + ' °C';
    } catch(e) {
        console.error(e);
    }
}

async function updateCPUStatus(){
    try {
        const r = await fetch('/cpu');
        if (!r.ok) return;
        const d = await r.json();

        // Поддержка одного или двух ядер
        if (d.core0 !== undefined) document.getElementById('cpu0').innerText = d.core0.toFixed(1) + ' %';
        if (d.core1 !== undefined) document.getElementById('cpu1').innerText = d.core1.toFixed(1) + ' %';
    } catch(e) {
        console.error("CPU update error:", e);
    }
}
window.addEventListener('DOMContentLoaded', () => {
    updatePrinterStatus();
    updateCPUStatus();
    setInterval(updatePrinterStatus, 500);
    setInterval(updateCPUStatus, 500);
});
</script>

<h2>Printer State:</h2>
<label>State: </label><span id="state">--</span><br><br>
<label>Progress: </label><span id="progress">0 %</span><br><br>
<label>Nozzle: </label><span id="nozzle">0 °C</span><br><br>
<label>Bed: </label><span id="bed">0 °C</span><br><br>
<label>ESP CPU Core: </label><span id="cpu0">0 %</span> / <span id="cpu1">0 %</span><br>
<hr>
)rawliteral";
// ===================================================================================================================================================
const char MAIN_PAGE_COLORSET[] PROGMEM = R"rawliteral(
<script>
document.addEventListener('DOMContentLoaded', () => {

  const colorInputs = document.querySelectorAll('input[type="color"]');
  const brightnessSlider = document.getElementById('brightness');
  const brightnessValue = document.getElementById('brightness_value'); //
  const ColorZonePercentSlider = document.getElementById('tempColorZonePercent');
  const ColorZonePercentValue = document.getElementById('tempColorZonePercent_value'); //
  const standbyColor = document.getElementById('standby_color');
  const completeColor = document.getElementById('complete_color');
  const bedTempCheckbox_st = document.getElementById('bed_temp_color_st');
  const bedTempCheckbox_co = document.getElementById('bed_temp_color_co');

  const breathPeriod = document.getElementById('breath_period_ms');
  const breathAmp = document.getElementById('breath_amp_percent');

  let previewTimer;
  let lastColorForPreview = standbyColor.value;

  function clampInput(input, min, max) {
    let v = parseInt(input.value);
    if (isNaN(v)) v = min;
    if (v < min) v = min;
    if (v > max) v = max;
    input.value = v;
  }

  function sendPreview(colorHex, e) {
    color = colorHex.replace("#", "");
    const brightness = brightnessSlider.value;
    const period = breathPeriod.value;
    const amp = breathAmp.value;

    let tempzone = 0;

    if (e) {
        // определяем по id вызвавшего элемента
        const id = e.target.id;

        if (id === 'standby_color') {
            color = standbyColor.value.replace("#", "");
            if (bedTempCheckbox_st.checked) {
              tempzone = ColorZonePercentSlider.value;
            }
        } else if (id === 'complete_color') {
          color = completeColor.value.replace("#", "");
          if (bedTempCheckbox_co.checked) {
            tempzone = ColorZonePercentSlider.value;
          }
        }
    }

    fetch(`/preview?color=${color}&brightness=${brightness}&period=${period}&amp=${amp}&tempzone=${tempzone}`)
        .catch(err => console.log("Preview error:", err));
}

  bedTempCheckbox_st.addEventListener('change', () => {
    debouncePreview(lastColorForPreview, { target: standbyColor });
  });
  bedTempCheckbox_co.addEventListener('change', () => {
    debouncePreview(lastColorForPreview, { target: completeColor });
  });

  function debouncePreview(colorHex, e) {
    clearTimeout(previewTimer);
    previewTimer = setTimeout(() => {
      clampInput(breathPeriod, 50, 10000);
      
      clampInput(breathAmp, 0, 100);
      sendPreview(colorHex, e);
    }, 500);
  }

  function updateColorPreview(e) {
    const preview = document.getElementById(e.target.id + '_preview');
    if (preview) preview.style.background = e.target.value;

    lastColorForPreview = e.target.value;
    debouncePreview(lastColorForPreview, e);
  }

  colorInputs.forEach(input => {
    input.addEventListener('input', updateColorPreview);

    const preview = document.getElementById(input.id + '_preview');
    if (preview) preview.style.background = input.value;
  });

  brightnessSlider.addEventListener('input', (e) => {
    brightnessValue.innerText = e.target.value + ' %';
    debouncePreview(lastColorForPreview);
  });

  ColorZonePercentSlider.addEventListener('input', (e) => {
    ColorZonePercentValue.innerText = e.target.value + ' %';
    debouncePreview(lastColorForPreview, { target: standbyColor });
  });

  breathPeriod.addEventListener('input', () => {
    debouncePreview(lastColorForPreview);
  });

  breathAmp.addEventListener('input', () => {
    debouncePreview(lastColorForPreview);
  });

  breathPeriod.addEventListener('change', () => clampInput(breathPeriod, 50, 10000));
  breathAmp.addEventListener('change', () => clampInput(breathAmp, 0, 100));

  brightnessValue.innerText = brightnessSlider.value + ' %';
});
</script>
<h2>Color Config:</h2>
<form method="POST" action="/saveColor">
  <label>Brightness:</label>
  <input type="range" min="0" max="100" 
         name="brightness" id="brightness" value="{{brightness}}">
  <span id="brightness_value">{{brightness}} %</span>
  <br>
  <label>Standby color:</label>
  <input type="color" style="width:50px;height:30px;border:none;" name="standby_color" id="standby_color" value="{{standby_color}}">
  <span id="standby_color_preview"></span>&nbsp;&nbsp;Bed temp color on standby:<input type="checkbox" name="bed_temp_color_st" id="bed_temp_color_st" {{bed_temp_color_st_checked}}><br>
  <label>Printing color:</label>
  <input type="color" style="width:50px;height:30px;border:none;" name="print_color" id="print_color" value="{{print_color}}">
  <span id="print_color_preview"></span> => 
  <input type="color" style="width:50px;height:30px;border:none;" name="print_color2" id="print_color2" value="{{print_color2}}">
  <span id="print_color2_preview"></span>&nbsp;State at center: <input type="checkbox" name="print_center_start" id="print_center_start" {{print_center_start_checked}}><br>
  <label>Pause color:</label>
  <input type="color" style="width:50px;height:30px;border:none;" name="pause_color" id="pause_color" value="{{pause_color}}">
  <span id="pause_color_preview"></span>Wave effect: <input type="checkbox" name="pause_wave_effect" id="pause_wave_effect" {{pause_wave_effect_checked}}><br>
  <label>Error color:</label>
  <input type="color" style="width:50px;height:30px;border:none;" name="error_color" id="error_color" value="{{error_color}}">
  <span id="error_color_preview"></span>Wave effect: <input type="checkbox" name="error_wave_effect" id="error_wave_effect" {{error_wave_effect_checked}}><br>
  <label>Complete color:</label>
  <input type="color" style="width:50px;height:30px;border:none;" name="complete_color" id="complete_color" value="{{complete_color}}">
  <span id="complete_color_preview"></span>Wave effect: <input type="checkbox" name="complete_wave_effect" id="complete_wave_effect" {{complete_wave_effect_checked}}>&nbsp;&nbsp;&nbsp;Bed temp color on complete:<input type="checkbox" name="bed_temp_color_co" id="bed_temp_color_co" {{bed_temp_color_co_checked}}><br>
  <br>
  <label>Breath effect (global):</label>  Period(ms):<input type="number" name="breath_period_ms" id="breath_period_ms" value="{{breath_period_ms}}">&nbsp;Amplitude (%): <input type="number" name="breath_amp_percent" id="breath_amp_percent" value="{{breath_amp_percent}}"> &nbsp;0% - off<br>
  <br>
  <label>Max temperature zone on LED(%):</label> <input type="range" min="0" max="70" name="tempColorZonePercent" id="tempColorZonePercent" value="{{tempColorZonePercent}}">
  <span id="tempColorZonePercent_value">{{tempColorZonePercent}} %</span>
  <br>
 <br>
  <input type="submit" value="Save Color Config (Reboot)">
</form>
<hr>
)rawliteral";
// ===================================================================================================================================================
const char MAIN_PAGE_BOTTOM[] PROGMEM = R"rawliteral(
  </body>
  </html> 
)rawliteral";


// ===================================================================================================================================================
const char LED_CONFIG_PAGE[] PROGMEM = R"rawliteral(
<h2>LED Config:</h2>
<form method="POST" action="/saveLed">

<label>LED Data Pin:</label>
<select name="led_pin" id="led_pin">
  <option value="4"  {{4_SELECTED}}>GPIO4</option>
  <option value="5"  {{5_SELECTED}}>GPIO5</option>
  <option value="1"  {{1_SELECTED}}>GPIO1</option>
  <option value="2"  {{2_SELECTED}}>GPIO2</option>
</select><br><br>

<label>LED Type:</label>
<select name="led_type" id="led_type">
  <option value="WS2812B" {{WS2812B_SELECTED}}>WS2812B</option>
  <option value="WS2812" {{WS2812_SELECTED}}>WS2812</option>
  <option value="WS2813" {{WS2813_SELECTED}}>WS2813</option>
  <option value="SK6812" {{SK6812_SELECTED}}>SK6812</option>
</select><br><br>

<label>LED Color Order:</label>
<select name="led_type_color" id="led_type_color">
  <option value="GRB" {{GRB_SELECTED}}>GRB</option>
  <option value="RGB" {{RGB_SELECTED}}>RGB</option>
  <option value="BGR" {{BGR_SELECTED}}>BGR</option>
  <option value="RBG" {{RBG_SELECTED}}>RBG</option>
</select><br><br>

<label>LED Count:</label>
<input type="number" name="led_count" id="led_count" value="{{led_count}}"><br><br>

<label>LED Right to Left:</label>
<input type="checkbox" name="led_right" id="led_right" {{led_right_checked}}><br><br>

<label>MAX Current:</label> <span id="led_current">0.4</span> A<br><br>

<input type="submit" value="Save LED Config (Reboot)">
<hr>
</form>

<script>
document.addEventListener('DOMContentLoaded', () => {
  const ledCount = document.getElementById('led_count');
  const currentLabel = document.getElementById('led_current');

  function updateCurrent() {
    const count = parseInt(ledCount.value) || 0;
    const amp = 0.4 + (count * 0.033);
    currentLabel.innerText = amp.toFixed(2);
  }

  ledCount.addEventListener('input', updateCurrent);
  updateCurrent();
});
</script>
)rawliteral";
// ===================================================================================================================================================
const char PRINTER_IP_PAGE[] PROGMEM = R"rawliteral(
<h2><p style="color:red">Printer not responding</p></h2><br><hr>
<h2>Printer Settings</h2>
<form method="POST" action="/savePrint">
  <label>Printer IP: </label><input name="ip" id="printer_ip" value="{{IP}}"><br>
  <input type="submit" value="Save">
</form><br>
<hr>
)rawliteral";

// ============================================================================================================================================================
const char WIFI_PAGE[] PROGMEM = R"rawliteral(
<script>

let pollTimer = null;

async function scanNetworks(){
  try {
    const r = await fetch('/scan');
    if (!r.ok) return;
    const list = await r.json();
    const sel = document.getElementById('ssid_select');
    sel.innerHTML = '';

    list.forEach(item=>{
      const opt = document.createElement('option');
      opt.value = item.ssid;
      opt.text = item.ssid + ' (' + item.rssi + ' dBm)';
      sel.appendChild(opt);
    });
  } catch(e) {
    console.error(e);
  }
}

async function enableAP(){

  const statusDiv = document.getElementById('status');
  statusDiv.innerText = "Switching to AP mode...";

  try{

    const r = await fetch('/enable_ap',{method:'POST'});
    const data = await r.json();

    statusDiv.innerText = "AP Mode active. IP: " + data.ip;

  }catch(e){
    console.error(e);
  }
}

async function checkWifiStatus(){
  const apMode = document.getElementById('AP_mode');
  try {
    const r = await fetch('/wifi_status');
    if (!r.ok) return;

    const data = await r.json();
    const statusDiv = document.getElementById('status');

    if (data.status === "connecting") {
      statusDiv.innerText = "Connecting...";
    }else if (data.status === "connected") {
      statusDiv.innerText = "Connected! Redirecting to " + data.ip;
      clearInterval(pollTimer);

      setTimeout(()=>{
        window.location = "http://" + data.ip;
      }, 1000);
    }
    else if (data.status === "failed") {
      statusDiv.innerText = "Connection failed. Check password.";
      clearInterval(pollTimer);
    }

  } catch(e){
    console.error(e);
  }
}

async function submitWifiForm(event){
  event.preventDefault();

  const form = document.getElementById('wifiForm');
  const formData = new FormData(form);

  const statusDiv = document.getElementById('status');
  statusDiv.innerText = "Starting connection...";

  try {
    const r = await fetch('/saveWifi', {
      method: 'POST',
      body: new URLSearchParams(formData)
    });

    if (!r.ok) {
      statusDiv.innerText = "Error starting connection";
      return;
    }

    // начинаем polling
    pollTimer = setInterval(checkWifiStatus, 1000);

  } catch(e){
    statusDiv.innerText = "Request failed";
    console.error(e);
  }
}

window.addEventListener('DOMContentLoaded', () => {
  const ssidSelect = document.getElementById('ssid_select');
  //scanNetworks();
  document.getElementById('wifiForm')
           .addEventListener('submit', submitWifiForm);
});

</script>

<h2>WiFi Settings</h2>

<form id="wifiForm">
  <label>SSID:</label>
  <select id="ssid_select" name="ssid"></select>
  <button type="button" onclick="scanNetworks()">Scan</button>
  <br>

  <label>Password:</label>
  <input name="pass" type="password"><br><br>


  <input type="submit" value="Connect"><button type="button" onclick="enableAP()">Stay in AP Mode</button>
</form>

<br>
<div id="status" style="color:green;"></div>

)rawliteral";
// ============================================================================================================================================================
const char REBOOT_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>Rebooting</title>
<script>
setTimeout(function(){
    window.location.href = "/";
}, 10000);
</script>
</head>
<body>
<h2>Settings saved.</h2>
<p>Device is rebooting...</p>
<p>You will be redirected in 10 seconds.</p>
</body>
</html>
)rawliteral";
// ============================================================================================================================================================
const char OTA_PAGE[] PROGMEM = R"rawliteral(
<hr>

<h2>Factory Reset</h2>
<button type="button" onclick="factoryReset()">Factory Reset and Reboot</button>

<hr>
<h2>Firmware Update</h2>
<form method="POST" action="/update" enctype="multipart/form-data">
  <input type="file" name="update">
  <input type="submit" value="Update Firmware">
</form>

<br>
<div id="status"></div>

<script>
async function factoryReset(){

  if(!confirm("Reset all settings and reboot?"))
      return;

  if(!confirm("This will erase all settings. Continue?"))
    return;

  const status = document.getElementById("status");
  status.innerText = "Resetting device...";

  try{

    const r = await fetch('/reset', {method:'POST'});

    if(!r.ok){
        status.innerText = "Reset failed";
        return;
    }

    status.innerText = "Device rebooting...";

  }catch(e){
    status.innerText = "Request failed";
  }
}

const form = document.querySelector('form');
form.addEventListener('submit', function(e){
  document.getElementById("status").innerText = "Uploading...";
});
</script>

<hr>
)rawliteral";