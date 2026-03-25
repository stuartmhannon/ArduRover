#include <Servo.h>
#include <Wire.h>
#include <TinyGPS++.h>
#include <HMC5883L.h>
#include "Arduino_LED_Matrix.h"

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

// Objects
Servo steeringServo, esc;
TinyGPSPlus gps;
HMC5883L compass;
ArduinoLEDMatrix matrix;

// Matrix Bitmaps (G and C)
const uint32_t G_LOGO[] = { 0x1f820420, 0x43840420, 0x43e00000 };
const uint32_t C_LOGO[] = { 0x1f820420, 0x40040420, 0x41f80000 };
const uint32_t BLANK[]  = { 0, 0, 0 };

// State Variables
int currentThrottle = THROTTLE_NEUTRAL;
int currentSteering = STEERING_CENTER;
int waypointCount = 0;
bool navigatingToWaypoint = false;
int targetWaypointIndex = 0;
unsigned long lastFlash = 0;
bool flashState = false;

// Serial communication
const byte CMD_FORWARD = 0x01;
const byte CMD_REVERSE = 0x02;
const byte CMD_LEFT = 0x03;
const byte CMD_RIGHT = 0x04;
const byte CMD_STOP = 0x05;
const byte CMD_RECORD = 0x06;
const byte CMD_START_NAV = 0x07;
const byte CMD_CLEAR = 0x08;
const byte CMD_STATUS_REQ = 0x09;

struct GPSPoint { double lat; double lng; };
const int MAX_WAYPOINTS = 20;
GPSPoint waypoints[MAX_WAYPOINTS];

void handleCommand(byte cmd);
void navigateToWaypoint();
void navigateToPoint(GPSPoint target);
float calculateBearing(double lat1, double lon1, double lat2, double lon2);
float distanceTo(GPSPoint target);
void updateLEDMatrix();
void sendStatus();

void setup() {
  Serial.begin(115200);
  Wire.begin();
  matrix.begin();

  compass.setScale(1.3);
  compass.setMeasurementMode(MEASUREMENT_CONTINUOUS);
  Serial1.begin(9600);

  steeringServo.attach(STEERING_PIN);
  esc.attach(ESC_PIN);
  steeringServo.writeMicroseconds(STEERING_CENTER);
  esc.writeMicroseconds(THROTTLE_NEUTRAL);
}

void loop() {
  while (Serial1.available() > 0) { gps.encode(Serial1.read()); }

  updateLEDMatrix();

  // Handle commands from ESP32S3 via Serial
  if (Serial.available()) {
    byte cmd = Serial.read();
    handleCommand(cmd);
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

  if (millis() - lastFlash > 500) {
    flashState = !flashState;
    lastFlash = millis();
  }

  if (!gpsOK) {
    if (flashState) matrix.loadFrame(G_LOGO);
    else matrix.loadFrame(BLANK);
  } else if (!compOK) {
    if (flashState) matrix.loadFrame(C_LOGO);
    else matrix.loadFrame(BLANK);
  } else {
    if (millis() % 4000 < 2000) matrix.loadFrame(G_LOGO);
    else matrix.loadFrame(C_LOGO);
  }
}

void handleCommand(byte cmd) {
  switch (cmd) {
    case CMD_FORWARD:
      currentThrottle = THROTTLE_FORWARD;
      currentSteering = STEERING_CENTER;
      navigatingToWaypoint = false;
      break;
    case CMD_REVERSE:
      currentThrottle = THROTTLE_REVERSE;
      currentSteering = STEERING_CENTER;
      navigatingToWaypoint = false;
      break;
    case CMD_LEFT:
      currentSteering = STEERING_LEFT;
      break;
    case CMD_RIGHT:
      currentSteering = STEERING_RIGHT;
      break;
    case CMD_STOP:
      currentThrottle = THROTTLE_NEUTRAL;
      currentSteering = STEERING_CENTER;
      navigatingToWaypoint = false;
      break;
    case CMD_RECORD:
      if (gps.location.isValid() && waypointCount < MAX_WAYPOINTS) {
        waypoints[waypointCount++] = {gps.location.lat(), gps.location.lng()};
      }
      break;
    case CMD_START_NAV:
      if (waypointCount > 0) {
        targetWaypointIndex = 0;
        navigatingToWaypoint = true;
      }
      break;
    case CMD_CLEAR:
      waypointCount = 0;
      navigatingToWaypoint = false;
      currentThrottle = THROTTLE_NEUTRAL;
      break;
    case CMD_STATUS_REQ:
      sendStatus();
      break;
  }
}

void navigateToWaypoint() {
  if (targetWaypointIndex >= waypointCount) {
    navigatingToWaypoint = false;
    currentThrottle = THROTTLE_NEUTRAL;
    currentSteering = STEERING_CENTER;
    return;
  }
  navigateToPoint(waypoints[targetWaypointIndex]);
  if (distanceTo(waypoints[targetWaypointIndex]) < WAYPOINT_RADIUS) {
    targetWaypointIndex++;
  }
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

void sendStatus() {
  MagnetometerScaled scaled = compass.readScaledAxis();
  bool compOK = (scaled.XAxis != 0 || scaled.YAxis != 0);
  float h = atan2(scaled.YAxis, scaled.XAxis) * 180 / PI;
  if (h < 0) h += 360;

  // Format: LAT#LNG#SPD#HDG#GPS_OK#COMP_OK#WP_COUNT#
  Serial.print(gps.location.lat(), 6);
  Serial.print("#");
  Serial.print(gps.location.lng(), 6);
  Serial.print("#");
  Serial.print(gps.speed.mps(), 1);
  Serial.print("#");
  Serial.print((int)h);
  Serial.print("#");
  Serial.print(gps.location.isValid() ? "1" : "0");
  Serial.print("#");
  Serial.print(compOK ? "1" : "0");
  Serial.print("#");
  Serial.println(waypointCount);
}
