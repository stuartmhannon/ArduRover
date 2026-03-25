# ArduRover — Autonomous RC Truck

## Hardware
- **Board:** Arduino Uno R4 Wifi (dual-core: RA4M1 + ESP32S3 for WiFi)
- **Motor:** PWM-controlled ESC
- **Sensors:** compass (magnetometer HMC5883L), GPS (via Serial1)
- **LED Matrix display** (ArduinoLEDMatrix library)

## Current Software
- **Main sketch:** `/Users/stuarthannon/ArduRover/Main_WebApp/Main_WebApp.ino`
- Arduino IDE (.ino sketches)
- Libraries: Servo, WiFiS3, Wire, TinyGPS++, HMC5883L, ArduinoLEDMatrix

## Current WiFi Implementation (RA4M1)
- Uses WiFiS3 library on main RA4M1 core
- SSID: "tellmywifiloveher"
- Web server on port 80
- Simple HTML control page + JSON status API

## Features
- Manual control: forward, reverse, left, right, stop
- Waypoint recording (up to 20 points using GPS)
- Autonomous navigation to waypoints
- LED Matrix status display (G=GPS status, C=Compass status)
- JSON status endpoint at `/api/status`
- Control endpoint at `/control?cmd=<command>`

## Pins & Constants
- Steering: pin 9, center=1500µs, left=1300µs, right=1700µs
- ESC: pin 10, neutral=1500µs, forward=1650µs, reverse=1350µs
- Serial1: GPS at 9600 baud

## Architecture Refactor

### Goal
Move WiFi/web server from RA4M1 to ESP32S3 directly.

### Proposed Split
- **RA4M1 (main core):** Motor control, sensor reading, navigation logic
- **ESP32S3:** WiFi, web server, webpage serving, Serial communication with RA4M1

### Inter-Core Communication
- Serial connection between ESP32S3 and RA4M1
- ESP32S3 sends commands to RA4M1
- RA4M1 sends status back to ESP32S3

## Preferences
- Always use Arduino IDE and Arduino coding conventions
- Always use GitHub to mirror projects

## Status
In progress — basic functionality working, refactoring to split WiFi to ESP32S3
