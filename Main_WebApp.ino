#include <Servo.h>
#include <WiFiS3.h>
#include <WiFiServer.h>
#include <Wire.h>
#include <TinyGPS++.h>
#include <HMC5883L.h>
#include "Arduino_LED_Matrix.h" // Add the LED Matrix library

// WiFi credentials
const char* ssid = "tellmywifiloveher";
const char* password = "domestictexts";

// Objects
Servo steeringServo, esc;
TinyGPSPlus gps;
HMC5883L compass;
WiFiServer server(80);
ArduinoLEDMatrix matrix;

// Matrix Bitmaps (G and C)
const uint32_t G_LOGO[] = { 0x1f820420, 0x43840420, 0x43e00000 };
const uint32_t C_LOGO[] = { 0x1f820420, 0x40040420, 0x41f80000 };
const uint32_t BLANK[]  = { 0, 0, 0 };

// Pins & Constants
const int STEERING_PIN = 9;
const int ESC_PIN = 10;
const int STEERING_CENTER = 1500;
const int STEERING_LEFT = 1300;
const int STEERING_RIGHT = 1700;
const int THROTTLE_NEUTRAL = 1500;
const int THROTTLE_FORWARD = 1650;
const int THROTTLE_REVERSE = 1350;
const float WAYPOINT_RADIUS = 5.0; 

// State Variables
int currentThrottle = THROTTLE_NEUTRAL;
int currentSteering = STEERING_CENTER;
int waypointCount = 0;
bool navigatingToWaypoint = false;
int targetWaypointIndex = 0;
unsigned long lastFlash = 0;
bool flashState = false;

struct GPSPoint { double lat; double lng; };
const int MAX_WAYPOINTS = 20; 
GPSPoint waypoints[MAX_WAYPOINTS];

// Prototypes
void handleControl(String request);
void sendWebPage(WiFiClient client);
void navigateToWaypoint();
void navigateToPoint(GPSPoint target);
float calculateBearing(double lat1, double lon1, double lat2, double lon2);
float distanceTo(GPSPoint target);
String buildStatusJSON();
void updateLEDMatrix();

void setup() {
  Serial.begin(9600);
  Wire.begin();
  matrix.begin(); // Start LED Matrix
  
  compass.setScale(1.3);
  compass.setMeasurementMode(MEASUREMENT_CONTINUOUS);
  Serial1.begin(9600);
  
  steeringServo.attach(STEERING_PIN);
  esc.attach(ESC_PIN);
  steeringServo.writeMicroseconds(STEERING_CENTER);
  esc.writeMicroseconds(THROTTLE_NEUTRAL);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  server.begin();
}

void loop() {
  while (Serial1.available() > 0) { gps.encode(Serial1.read()); }
  
  updateLEDMatrix(); // Run the visual health check

  WiFiClient client = server.available();
  if (client) {
    String request = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        request += c;
        if (request.endsWith("\r\n\r\n")) break;
      }
    }
    
    if (request.indexOf("GET /api/status") != -1) {
      client.println("HTTP/1.1 200 OK\r\nContent-type: application/json\r\nConnection: close\r\n\r\n" + buildStatusJSON());
    } else if (request.indexOf("GET /control?") != -1) {
      handleControl(request);
      client.println("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nOK");
    } else if (request.indexOf("GET / ") != -1) {
      sendWebPage(client);
    }
    delay(10);
    client.stop();
  }
  
  if (navigatingToWaypoint) { navigateToWaypoint(); }

  steeringServo.writeMicroseconds(currentSteering);
  esc.writeMicroseconds(currentThrottle);
  delay(50);
}

void updateLEDMatrix() {
  MagnetometerScaled scaled = compass.readScaledAxis();
  bool compOK = (scaled.XAxis != 0 || scaled.YAxis != 0);
  bool gpsOK = gps.location.isValid();

  // Toggle flash every 500ms
  if (millis() - lastFlash > 500) {
    flashState = !flashState;
    lastFlash = millis();
  }

  // Logic: Show G if GPS is bad, then show C. 
  // If GPS is bad, G flashes. If GPS is good, G is solid.
  if (!gpsOK) {
    if (flashState) matrix.loadFrame(G_LOGO);
    else matrix.loadFrame(BLANK);
  } else if (!compOK) {
    if (flashState) matrix.loadFrame(C_LOGO);
    else matrix.loadFrame(BLANK);
  } else {
    // Everything OK? Display a steady smiley or a steady "OK"
    // Here we will just alternate between G and C slowly to show both are fine
    if (millis() % 4000 < 2000) matrix.loadFrame(G_LOGO);
    else matrix.loadFrame(C_LOGO);
  }
}

// ... Keep handleControl, buildStatusJSON, and Navigation functions from previous version ...

void handleControl(String request) {
  int cmdStart = request.indexOf("cmd=") + 4;
  int spaceIndex = request.indexOf(" ", cmdStart);
  String cmd = (spaceIndex == -1) ? request.substring(cmdStart) : request.substring(cmdStart, spaceIndex);
  
  if (cmd == "forward") { currentThrottle = THROTTLE_FORWARD; currentSteering = STEERING_CENTER; navigatingToWaypoint = false; }
  else if (cmd == "reverse") { currentThrottle = THROTTLE_REVERSE; currentSteering = STEERING_CENTER; navigatingToWaypoint = false; }
  else if (cmd == "left") { currentSteering = STEERING_LEFT; }
  else if (cmd == "right") { currentSteering = STEERING_RIGHT; }
  else if (cmd == "stop") { currentThrottle = THROTTLE_NEUTRAL; currentSteering = STEERING_CENTER; navigatingToWaypoint = false; }
  else if (cmd == "record" && gps.location.isValid() && waypointCount < MAX_WAYPOINTS) {
    waypoints[waypointCount++] = {gps.location.lat(), gps.location.lng()};
  }
  else if (cmd == "start_nav") { if (waypointCount > 0) { targetWaypointIndex = 0; navigatingToWaypoint = true; } }
  else if (cmd == "clear") { waypointCount = 0; navigatingToWaypoint = false; currentThrottle = THROTTLE_NEUTRAL; }
}

void navigateToWaypoint() {
  if (targetWaypointIndex >= waypointCount) {
    navigatingToWaypoint = false;
    currentThrottle = THROTTLE_NEUTRAL;
    currentSteering = STEERING_CENTER;
    return;
  }
  navigateToPoint(waypoints[targetWaypointIndex]);
  if (distanceTo(waypoints[targetWaypointIndex]) < WAYPOINT_RADIUS) { targetWaypointIndex++; }
}

void navigateToPoint(GPSPoint target) {
  if (!gps.location.isValid()) return;
  float bearing = calculateBearing(gps.location.lat(), gps.location.lng(), target.lat, target.lng);
  MagnetometerScaled scaled = compass.readScaledAxis();
  float heading = atan2(scaled.YAxis, scaled.XAxis) * 180 / PI;
  float declinationAngle = -12.67;
  heading += declinationAngle;
  if (heading < 0) heading += 360;
  if (heading > 360) heading -= 360;
  float diff = bearing - heading;
  if (diff > 180) diff -= 360;
  if (diff < -180) diff += 360;
  currentSteering = (diff > 15) ? STEERING_RIGHT : (diff < -15 ? STEERING_LEFT : STEERING_CENTER);
  currentThrottle = THROTTLE_FORWARD;
}

float calculateBearing(double lat1, double lon1, double lat2, double lon2) {
  double dLon = (lon2 - lon1) * PI / 180.0;
  double y = sin(dLon) * cos(lat2 * PI / 180.0);
  double x = cos(lat1 * PI / 180.0) * sin(lat2 * PI / 180.0) - sin(lat1 * PI / 180.0) * cos(lat2 * PI / 180.0) * cos(dLon);
  double b = atan2(y, x) * 180.0 / PI;
  return (b < 0) ? b + 360 : b;
}

float distanceTo(GPSPoint target) {
  return (float)TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), target.lat, target.lng);
}

String buildStatusJSON() {
  MagnetometerScaled scaled = compass.readScaledAxis();
  bool compOK = (scaled.XAxis != 0 || scaled.YAxis != 0);
  float h = atan2(scaled.YAxis, scaled.XAxis) * 180 / PI;
  if (h < 0) h += 360;
  String json = "{";
  json += "\"lat\":" + String(gps.location.lat(), 6) + ",\"lng\":" + String(gps.location.lng(), 6) + ",\"spd\":" + String(gps.speed.mph(), 1) + ",";
  json += "\"hdg\":" + String(h, 0) + ",\"gps_ok\":" + String(gps.location.isValid() ? "true" : "false") + ",";
  json += "\"comp_ok\":" + String(compOK ? "true" : "false") + ",\"wp\":" + String(waypointCount) + "}";
  return json;
}

void sendWebPage(WiFiClient client) {
  client.println("HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n");
  client.println("<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>");
  client.println("body{font-family:sans-serif;text-align:center;background:#f4f4f4;} .btn{width:80px;height:50px;margin:5px;font-weight:bold;}");
  client.println(".stat-box{background:white;padding:10px;margin:10px auto;width:90%;border-radius:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1);}");
  client.println(".ok{color:green;} .fail{color:red;}</style></head><body>");
  client.println("<div class='stat-box'><h3>System Status</h3>GPS: <span id='g_s'>-</span> | Comp: <span id='c_s'>-</span><br>Pos: <span id='p'>-</span><br>Hdg: <span id='h'>0</span>&deg; | Spd: <span id='s'>0</span> mph | WPs: <span id='w'>0</span></div>");
  client.println("<button class='btn' onclick=\"f('forward')\">FWD</button><br><button class='btn' onclick=\"f('left')\">LEFT</button><button class='btn' onclick=\"f('stop')\" style='background:red;color:white;'>STOP</button><button class='btn' onclick=\"f('right')\">RIGHT</button><br><button class='btn' onclick=\"f('reverse')\">REV</button><hr>");
  client.println("<h3>Waypoints</h3><button class='btn' onclick=\"f('record')\" style='background:green;color:white;'>RECORD</button><button class='btn' onclick=\"f('start_nav')\" style='background:blue;color:white;'>NAV</button><button class='btn' onclick=\"f('clear')\" style='background:orange;'>CLEAR</button>");
  client.println("<script>function f(c){fetch('/control?cmd='+c);} setInterval(()=>{fetch('/api/status').then(r=>r.json()).then(d=>{");
  client.println("document.getElementById('g_s').innerHTML=d.gps_ok?'OK':'LOCKING'; document.getElementById('g_s').className=d.gps_ok?'ok':'fail';");
  client.println("document.getElementById('c_s').innerHTML=d.comp_ok?'OK':'FAIL'; document.getElementById('c_s').className=d.comp_ok?'ok':'fail';");
  client.println("document.getElementById('p').innerHTML=d.lat.toFixed(4)+','+d.lng.toFixed(4); document.getElementById('h').innerHTML=d.hdg;");
  client.println("document.getElementById('s').innerHTML=d.spd; document.getElementById('w').innerHTML=d.wp; });}, 1000);</script></body></html>");
}