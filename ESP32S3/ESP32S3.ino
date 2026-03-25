#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>

// WiFi credentials
const char* ssid = "tellmywifiloveher";
const char* password = "domestictexts";

// Serial connection to RA4M1
HardwareSerial RA4M1(0); // UART0 on standard pins

// Commands to RA4M1
const byte CMD_FORWARD = 0x01;
const byte CMD_REVERSE = 0x02;
const byte CMD_LEFT = 0x03;
const byte CMD_RIGHT = 0x04;
const byte CMD_STOP = 0x05;
const byte CMD_RECORD = 0x06;
const byte CMD_START_NAV = 0x07;
const byte CMD_CLEAR = 0x08;
const byte CMD_STATUS_REQ = 0x09;

WebServer server(80);

// Status from RA4M1
float currentLat = 0;
float currentLng = 0;
float currentSpeed = 0;
int currentHeading = 0;
bool gpsOK = false;
bool compOK = false;
int waypointCount = 0;
unsigned long lastStatusRequest = 0;

void handleRoot();
void handleApiStatus();
void handleControl();
void requestStatus();

void setup() {
  Serial.begin(115200);
  RA4M1.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  server.on("/", handleRoot);
  server.on("/api/status", handleApiStatus);
  server.on("/control", handleControl);
  server.begin();
}

void loop() {
  server.handleClient();

  // Poll RA4M1 for status every second
  if (millis() - lastStatusRequest > 1000) {
    requestStatus();
    lastStatusRequest = millis();
  }

  // Read status response from RA4M1
  while (RA4M1.available()) {
    String response = RA4M1.readStringUntil('\n');
    parseStatus(response);
  }
}

void parseStatus(String data) {
  int idx = 0;
  int start = 0;

  for (int i = 0; i < 7; i++) {
    idx = data.indexOf('#', start);
    if (idx == -1) return;

    String val = data.substring(start, idx);
    switch (i) {
      case 0: currentLat = val.toFloat(); break;
      case 1: currentLng = val.toFloat(); break;
      case 2: currentSpeed = val.toFloat(); break;
      case 3: currentHeading = val.toInt(); break;
      case 4: gpsOK = (val == "1"); break;
      case 5: compOK = (val == "1"); break;
      case 6: waypointCount = val.toInt(); break;
    }
    start = idx + 1;
  }
}

void requestStatus() {
  RA4M1.write(CMD_STATUS_REQ);
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:sans-serif;text-align:center;background:#f4f4f4;}";
  html += ".btn{width:80px;height:50px;margin:5px;font-weight:bold;}";
  html += ".stat-box{background:white;padding:10px;margin:10px auto;width:90%;border-radius:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1);}";
  html += ".ok{color:green;} .fail{color:red;}</style></head><body>";
  html += "<div class='stat-box'><h3>System Status</h3>";
  html += "GPS: <span id='g_s'>-</span> | Comp: <span id='c_s'>-</span><br>";
  html += "Pos: <span id='p'>-</span><br>";
  html += "Hdg: <span id='h'>0</span>&deg; | Spd: <span id='s'>0</span> m/s | WPs: <span id='w'>0</span>";
  html += "</div>";
  html += "<button class='btn' onclick=\"f('forward')\">FWD</button><br>";
  html += "<button class='btn' onclick=\"f('left')\">LEFT</button>";
  html += "<button class='btn' onclick=\"f('stop')\" style='background:red;color:white;'>STOP</button>";
  html += "<button class='btn' onclick=\"f('right')\">RIGHT</button><br>";
  html += "<button class='btn' onclick=\"f('reverse')\">REV</button>";
  html += "<hr><h3>Waypoints</h3>";
  html += "<button class='btn' onclick=\"f('record')\" style='background:green;color:white;'>RECORD</button>";
  html += "<button class='btn' onclick=\"f('start_nav')\" style='background:blue;color:white;'>NAV</button>";
  html += "<button class='btn' onclick=\"f('clear')\" style='background:orange;'>CLEAR</button>";
  html += "<script>";
  html += "function f(c){fetch('/control?cmd='+c);}";
  html += "setInterval(()=>{fetch('/api/status').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('g_s').innerHTML=d.gps_ok?'OK':'LOCKING';";
  html += "document.getElementById('g_s').className=d.gps_ok?'ok':'fail';";
  html += "document.getElementById('c_s').innerHTML=d.comp_ok?'OK':'FAIL';";
  html += "document.getElementById('c_s').className=d.comp_ok?'ok':'fail';";
  html += "document.getElementById('p').innerHTML=d.lat.toFixed(4)+','+d.lng.toFixed(4);";
  html += "document.getElementById('h').innerHTML=d.hdg;";
  html += "document.getElementById('s').innerHTML=d.spd;";
  html += "document.getElementById('w').innerHTML=d.wp;";
  html += "});},1000);";
  html += "</script></body></html>";
  server.send(200, "text/html", html);
}

void handleApiStatus() {
  String json = "{";
  json += "\"lat\":" + String(currentLat, 6) + ",";
  json += "\"lng\":" + String(currentLng, 6) + ",";
  json += "\"spd\":" + String(currentSpeed, 1) + ",";
  json += "\"hdg\":" + String(currentHeading) + ",";
  json += "\"gps_ok\":" + String(gpsOK ? "true" : "false") + ",";
  json += "\"comp_ok\":" + String(compOK ? "true" : "false") + ",";
  json += "\"wp\":" + String(waypointCount);
  json += "}";
  server.send(200, "application/json", json);
}

void handleControl() {
  String cmd = server.arg("cmd");
  if (cmd == "forward") RA4M1.write(CMD_FORWARD);
  else if (cmd == "reverse") RA4M1.write(CMD_REVERSE);
  else if (cmd == "left") RA4M1.write(CMD_LEFT);
  else if (cmd == "right") RA4M1.write(CMD_RIGHT);
  else if (cmd == "stop") RA4M1.write(CMD_STOP);
  else if (cmd == "record") RA4M1.write(CMD_RECORD);
  else if (cmd == "start_nav") RA4M1.write(CMD_START_NAV);
  else if (cmd == "clear") RA4M1.write(CMD_CLEAR);
  server.send(200, "text/plain", "OK");
}
