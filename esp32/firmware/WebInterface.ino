#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;
volatile int currentFps = 0;

void wifiSetup() {
  WiFi.softAP("RC-Controller");
  Serial.println(WiFi.softAPIP());

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", sendHtml);

  server.on("/stream", handleStream);

  server.on("/servo", []() {
    int ch = server.arg("ch").toInt();
    int val = server.arg("val").toInt();
    panTiltPwm.setPWM(ch, 0, mapServo(val));
    server.send(200, "text/plain", "OK");
  });

  server.on("/battery", []() {
    float battery = getBatteryPercentage();
    server.send(200, "text/plain", String(battery, 1) + "%");
  });

  server.on("/all_speed", []() {
    int val = server.arg("val").toInt();
    for (int i = 0; i < 4; i++) {
      motorSpeeds[i] = val;
      // Note: We no longer auto-set direction to 0 here to allow speed changes while moving
      applyMotor(i);
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/all_dir", []() {
    int d = server.arg("d").toInt();
    for (int i = 0; i < 4; i++) {
      motorDirs[i] = d;
      applyMotor(i);
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/turn", []() {
    String dir = server.arg("dir");
    if (dir == "R") {
      motorDirs[0] = 1;
      motorDirs[2] = 1;
      motorDirs[1] = -1;
      motorDirs[3] = -1;
    } else if (dir == "L") {
      motorDirs[0] = -1;
      motorDirs[2] = -1;
      motorDirs[1] = 1;
      motorDirs[3] = 1;
    }
    for (int i = 0; i < 4; i++) applyMotor(i);
    server.send(200, "text/plain", "OK");
  });

  server.on("/lateral", []() {
    String dir = server.arg("dir");
    if (dir == "R") {
      motorDirs[0] = 1;
      motorDirs[2] = -1;
      motorDirs[1] = -1;
      motorDirs[3] = 1;
    } else if (dir == "L") {
      motorDirs[0] = -1;
      motorDirs[2] = 1;
      motorDirs[1] = 1;
      motorDirs[3] = -1;
    }
    for (int i = 0; i < 4; i++) applyMotor(i);
    server.send(200, "text/plain", "OK");
  });

  server.on("/speed", []() {
    int id = server.arg("id").toInt();
    int val = server.arg("val").toInt();
    if (id >= 0 && id < 4) {
      motorSpeeds[id] = val;
      applyMotor(id);
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/dir", []() {
    int id = server.arg("id").toInt();
    int d = server.arg("d").toInt();
    if (id >= 0 && id < 4) {
      motorDirs[id] = d;
      applyMotor(id);
    }
    server.send(200, "text/plain", "OK");
  });

  // HANDLER FOR JOYSTICK & MIXED DRIVE
  server.on("/drive", []() {
    int x = server.arg("x").toInt();  // Strafe
    int y = server.arg("y").toInt();  // Forward/Back
    int z = server.arg("z").toInt();  // Turn

    // Mecanum Mixing Formulas
    // M0=FL, M1=FR, M2=RL, M3=RR
    int speeds[4];
    speeds[0] = y + x + z;  // FL
    speeds[1] = y - x - z;  // FR
    speeds[2] = y - x + z;  // RL
    speeds[3] = y + x - z;  // RR

    // Normalize speeds if they exceed 255
    int maxVal = 0;
    for (int i = 0; i < 4; i++) maxVal = max(maxVal, abs(speeds[i]));

    if (maxVal > 255) {
      for (int i = 0; i < 4; i++) speeds[i] = map(speeds[i], -maxVal, maxVal, -255, 255);
    }

    // Apply to motors
    for (int i = 0; i < 4; i++) {
      // Handle Deadzone
      if (abs(speeds[i]) < 15) {
        motorSpeeds[i] = 0;
        motorDirs[i] = 0;
      } else {
        motorDirs[i] = (speeds[i] > 0) ? 1 : -1;
        motorSpeeds[i] = abs(speeds[i]);
      }
      applyMotor(i);
    }
    server.send(200, "text/plain", "OK");
  });


  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
}

void wifiLoop() {
  dnsServer.processNextRequest();
  server.handleClient();
}

void handleStream() {
  WiFiClient client = server.client();

  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: " + String(_STREAM_CONTENT_TYPE) + "\r\n");
  client.print("Access-Control-Allow-Origin: *\r\n");
  client.print("\r\n");

  int frameCount = 0;
  unsigned long lastTime = millis();

  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      delay(100);
      continue;
    }

    client.print(_STREAM_BOUNDARY);
    char part_header[64];
    snprintf(part_header, 64, _STREAM_PART, fb->len);
    client.print(part_header);

    client.write(fb->buf, fb->len);
    client.print("\r\n");

    esp_camera_fb_return(fb);

    frameCount++;

    if (millis() - lastTime >= 1000) {
      currentFps = frameCount;
      frameCount = 0;
      lastTime = millis();
      Serial.printf("Stream FPS: %d\n", currentFps);
    }
  }
}
void sendHtml() {
  float battery = getBatteryPercentage();

  String htmlResponse = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no, viewport-fit=cover">
<style>
  body{overscroll-behavior-y: contain; font-family:sans-serif; text-align:center; background:#eee; padding-bottom: 50px; user-select: none; -webkit-user-select: none; -webkit-touch-callout: none;}
  input[type=range]{width:85%; height:25px; margin: 15px 0;}

  /* Card Styles */
  .m-card{background:white; margin:10px auto; padding:15px; width:90%; border-radius:10px; box-shadow:0 2px 5px rgba(0,0,0,0.1); border-style: solid; border-color: black;}
  .master-card{background:#2c3e50; color:white; padding: 0; overflow: hidden;} /* Removed padding for tabs */
  
  .grid-container { display: grid; grid-template-columns: 1fr 1fr; gap: 5px; max-width: 300px; margin: 0 auto; }
  
  /* Buttons */
  button{padding:15px; margin:5px; cursor:pointer; border-radius:8px; border:none; font-weight:bold; font-size: 14px; transition: 0.2s;}
  button:active { opacity: 0.7; transform: scale(0.95); }
  .btn-stop{background:#e74c3c; color:white; grid-column: span 2;}
  .btn-dir{background:#3498db; color:white;}
  .btn-turn{background:#f39c12; color:white;}

  /* --- TAB STYLES --- */
  .tab { overflow: hidden; border-bottom: 1px solid #34495e; background-color: #1a252f; }
  .tab button { background-color: inherit; float: left; border: none; outline: none; cursor: pointer; padding: 14px 16px; transition: 0.3s; font-size: 16px; color: #ccc; width: 50%; border-radius: 0; margin: 0;}
  .tab button:hover { background-color: #34495e; color: white;}
  .tab button.active { background-color: #2c3e50; color: white; border-bottom: 3px solid #f39c12; }
  .tabcontent { display: none; padding: 20px 15px; animation: fadeEffect 0.5s; }
  @keyframes fadeEffect { from {opacity: 0;} to {opacity: 1;} }

  /* JOYSTICK */
  #joystick-container {
    position: relative; width: 200px; height: 200px; margin: 0 auto; 
    background: rgba(255,255,255,0.1); border-radius: 50%; border: 2px solid #7f8c8d;
    touch-action: none; /* Prevents scrolling while using joystick */
  }
</style>
<script>
  // Battery Refresh
  setInterval(refreshBattery, 10000);
  function refreshBattery() {
    fetch('/battery').then(r => r.text()).then(v => {
        document.getElementById('battery').innerText = 'Battery: ' + v;
    });
  }

  // Tab Logic
  function openTab(evt, tabName) {
    var i, tabcontent, tablinks;

    tabcontent = document.getElementsByClassName("tabcontent");
    for (i = 0; i < tabcontent.length; i++) {
      tabcontent[i].style.display = "none";
    }

    tablinks = document.getElementsByClassName("tablinks");
    for (i = 0; i < tablinks.length; i++) {
      tablinks[i].className =
        tablinks[i].className.replace(" active", "");
    }

    document.getElementById(tabName).style.display = "block";
    evt.currentTarget.className += " active";

    const cam = document.getElementById("cameraFeed");

    if (tabName === "Camera") {

      cam.style.display = "block";

      // restart stream fresh every time
      cam.src = "/stream?ts=" + Date.now();

    } else {

      // stop stream
      cam.src = "";
      cam.style.display = "none";
    }
  }
</script>
<script>
  let lastServoSend = 0;
  let pendingServo = null;
  const SERVO_INTERVAL = 40; 


  function setServo(ch, val) {
    pendingServo = { ch, val };
    const now = Date.now();

    if (now - lastServoSend >= SERVO_INTERVAL) {
      sendServo();
    }
  }

  function sendServo() {
    if (!pendingServo) return;

    fetch(`/servo?ch=${pendingServo.ch}&val=${pendingServo.val}`);
    lastServoSend = Date.now();
    pendingServo = null;
  }
</script>
<script>
  // Updates visual sliders and ESP32 speed state without changing direction
  let lastSpeedSend = 0;
  let pendingSpeed = null;
  const SPEED_INTERVAL = 50; // ms (~20 Hz)

  function syncSpeed(val) {
    document.querySelectorAll('.motor-slider').forEach(s => s.value = val);
    document.getElementById('m-slider').value = val;

    pendingSpeed = val;
    const now = Date.now();

    if (now - lastSpeedSend >= SPEED_INTERVAL) {
      sendSpeed();
    }
  }

  function sendSpeed() {
    if (pendingSpeed === null) return;

    fetch('/all_speed?val=' + pendingSpeed);
    lastSpeedSend = Date.now();
    pendingSpeed = null;
  }

  // Movement: Applies current master slider value to all motors and sets direction
  function masterMove(d) {
    let s = document.getElementById('m-slider').value;
    syncSpeed(s); 
    fetch('/all_dir?d=' + d);
  }

  function masterTurn(t) {
    let s = document.getElementById('m-slider').value;
    syncSpeed(s);
    fetch('/turn?dir=' + t);
  }

  function masterLateral(t) {
    let s = document.getElementById('m-slider').value;
    syncSpeed(s);
    fetch('/lateral?dir=' + t);
  }
  
  // Stop: Kills direction but keeps the slider value visual and armed
  function masterStop() {
    fetch('/all_dir?d=0');
  }
</script>
<script>
  // --- DRIVE LOGIC ---
  let lastDriveSend = 0;
  let driveData = { x: 0, y: 0, z: 0 };
  let busy = false;

  function sendDrive(force = false) {
    // If not forcing, respect the busy flag and rate limit
    if (!force) {
      if (busy) return; 
      if (Date.now() - lastDriveSend < 50) return;
    }

    busy = true;
    fetch(`/drive?x=${driveData.x}&y=${driveData.y}&z=${driveData.z}`)
      .then(() => { 
        busy = false; 
        lastDriveSend = Date.now(); 
      })
      .catch(() => { 
        busy = false; 
      });
  }

  // --- JOYSTICK LOGIC ---
  let canvas, ctx;
  let width, height, radius, x_orig, y_orig;
  let coord = { x: 0, y: 0 };
  let paint = false;

  function initJoystick() {
    canvas = document.getElementById('joystick');
    ctx = canvas.getContext('2d');
    resize(); 

    // Mouse Events
    canvas.addEventListener('mousedown', startDraw);
    canvas.addEventListener('mousemove', draw);
    canvas.addEventListener('mouseup', stopDraw);
    canvas.addEventListener('mouseleave', stopDraw);
    
    // Touch Events
    canvas.addEventListener('touchstart', startDraw);
    canvas.addEventListener('touchmove', draw);
    canvas.addEventListener('touchend', stopDraw);
    canvas.addEventListener('touchcancel', stopDraw);

    drawBackground();
    drawStick(x_orig, y_orig);
  }

  function resize() {
    width = 200; height = 200; // Fixed size
    radius = 35; // Stick radius
    x_orig = width / 2;
    y_orig = height / 2;
  }

  function drawBackground() {
    ctx.beginPath();
    ctx.arc(x_orig, y_orig, x_orig - 10, 0, Math.PI * 2, true);
    ctx.fillStyle = '#34495e'; 
    ctx.fill();
  }

  function drawStick(x, y) {
    ctx.beginPath();
    ctx.arc(x, y, radius, 0, Math.PI * 2, true);
    ctx.fillStyle = '#e74c3c';
    ctx.fill();
    ctx.strokeStyle = '#c0392b';
    ctx.lineWidth = 4;
    ctx.stroke();
  }

  function getPosition(event) {
    var rect = canvas.getBoundingClientRect();
    var clientX = event.clientX || event.touches[0].clientX;
    var clientY = event.clientY || event.touches[0].clientY;
    return { x: clientX - rect.left, y: clientY - rect.top };
  }

  function startDraw(event) {
    paint = true;
    getPosition(event);
    draw(event);
  }

  function stopDraw() {
    paint = false;
    ctx.clearRect(0, 0, width, height);
    drawBackground();
    drawStick(x_orig, y_orig);
    
    // Reset Drive
    driveData.x = 0;
    driveData.y = 0;
    sendDrive(true);
  }

  function draw(event) {
    if (!paint) return;
    event.preventDefault(); // Stop scrolling
    var pos = getPosition(event);
    var x = pos.x; 
    var y = pos.y;
    
    // Calculate angle and distance
    var angle = Math.atan2((y - y_orig), (x - x_orig));
    var dist = Math.hypot(x - x_orig, y - y_orig);
    var max_dist = (width/2) - radius - 10;

    // Clamp stick inside circle
    if (dist > max_dist) {
      x = x_orig + max_dist * Math.cos(angle);
      y = y_orig + max_dist * Math.sin(angle);
    }
    
    ctx.clearRect(0, 0, width, height);
    drawBackground();
    drawStick(x, y);

    // Map to -255 to 255
    // X is Left/Right (Inverse for some setups, check signs)
    // Y is Up/Down (Canvas Y is down-positive, we need Up-positive)
    let valX = Math.round((x - x_orig) / max_dist * 255);
    let valY = Math.round((y_orig - y) / max_dist * 255); // Inverted Y

    driveData.x = valX; 
    driveData.y = valY;
    sendDrive();
  }

  // --- SLIDER RECENTER LOGIC ---
  function updateTurn(val) {
    driveData.z = parseInt(val) * 2.5; // Scale -100/100 to -250/250
    sendDrive();
  }
  
  function resetTurn(el) {
    el.value = 0;
    driveData.z = 0;
    sendDrive(true);
  }

  // --- SERVO LOGIC (KEEPING EXISTING) ---
  let lastServo = 0;
  function setServo(ch, val) {
    if(Date.now() - lastServo < 40) return;
    fetch(`/servo?ch=${ch}&val=${val}`);
    lastServo = Date.now();
  }
</script>
</head>
<body onload="document.getElementById('defaultOpen').click(); initJoystick();">

  <div class='m-card master-card' style="padding: 15px;">
    <p id="battery" style="margin:0; display:inline-block;">Battery: BATTERY%</p>
    <button class='btn-dir' style="padding: 5px 10px; margin-left:10px;" onclick="refreshBattery()">&#x21bb;</button>
  </div>

  <div class='m-card master-card'>
    
    <div class="tab">
      <button class="tablinks" onclick="openTab(event, 'Drive')" id="defaultOpen">DRIVE</button>
      <button class="tablinks" onclick="openTab(event, 'Analog')">ANALOG</button>
      <button class="tablinks" onclick="openTab(event, 'Camera')">CAMERA</button>
    </div>

    <div id="Drive" class="tabcontent">
      <h3>MASTER DRIVE</h3>
      <p>Speed</p>
      <input type='range' id='m-slider' min='0' max='255' value='150' oninput="syncSpeed(this.value)">
      
      <div class="grid-container">
        <button class='btn-dir' style="grid-column: span 2;" onclick="masterMove(1)">FORWARD</button>
        <button class='btn-turn' onclick="masterTurn('L')">TURN LEFT</button>
        <button class='btn-turn' onclick="masterTurn('R')">TURN RIGHT</button>
        <button class='btn-dir' onclick="masterLateral('L')">MOVE LEFT</button>
        <button class='btn-dir' onclick="masterLateral('R')">MOVE RIGHT</button>
        <button class='btn-dir' style="grid-column: span 2;" onclick="masterMove(-1)">BACKWARD</button>
        <button class='btn-stop' onclick="masterStop()">STOP ALL</button>
      </div>
    </div>

    <div id="Analog" class="tabcontent">
      <h3>POSITIONAL CONTROL</h3>
      
      <p>Rotation (Release to Center)</p>
      <input type="range" min="-100" max="100" value="0" 
             oninput="updateTurn(this.value)" 
             onmouseup="resetTurn(this)" 
             ontouchend="resetTurn(this)">

      <br><br>
      
      <div id="joystick-container">
        <canvas id="joystick" width="200" height="200"></canvas>
      </div>
      <p><small>Drag to Move / Slide</small></p>
    </div>

    <div id="Camera" class="tabcontent">
      <h3>Pan Tilt Control</h3>
      <p>Horizontal (Pan)</p>
      <input type="range" min="0" max="180" value="90" oninput="setServo(0,this.value)">

      <p>Vertical (Tilt)</p>
      <input type="range" min="0" max="180" value="90" oninput="setServo(1,this.value)">
      
      <br>
      <small>Center servos manually if required</small>

      <h3>Camera Feed</h3>

      <img id="cameraFeed"
          style="width:100%; max-width:600px; border-radius:10px; margin-bottom:15px; display:none;">

    </div>

  </div>


)rawliteral";

  for (int i = 0; i < 4; i++) {
    htmlResponse += "<div class='m-card'><h3>Motor " + String(i + 1) + (motors[i].isInverted ? " (Inv)" : "") + "</h3>";
    htmlResponse += "<input type='range' class='motor-slider' min='0' max='255' value='150' oninput=\"fetch('/speed?id=" + String(i) + "&val=' + this.value)\"><br>";
    htmlResponse += "<button class='btn-dir' onclick=\"fetch('/dir?id=" + String(i) + "&d=1')\">FWD</button>";
    htmlResponse += "<button class='btn-dir' onclick=\"fetch('/dir?id=" + String(i) + "&d=-1')\">BWD</button>";
    htmlResponse += "<button class='btn-stop' style='width: 80px;' onclick=\"fetch('/dir?id=" + String(i) + "&d=0')\">STOP</button></div>";
  }

  htmlResponse += "</body></html>";

  htmlResponse.replace("BATTERY%", String(battery, 1) + "%");
  server.send(200, "text/html", htmlResponse);
}