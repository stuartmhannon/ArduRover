# ArduRover — Autonomous RC Truck

## Hardware
- **Board:** Arduino Uno R4 Wifi (dual-core: RA4M1 + ESP32S3 for WiFi)
- **Motor:** PWM-controlled ESC
- **Sensors:** compass (magnetometer HMC5883L), GPS (via Serial1)
- **LED Matrix display** (ArduinoLEDMatrix library)

## Software Architecture (Split)

### RA4M1 (`/RA4M1/RA4M1.ino`)
- Motor control (Servo/ESC on pins 9, 10)
- GPS parsing (Serial1 at 9600 baud)
- Compass reading (I2C HMC5883L)
- LED Matrix status display
- Navigation logic (waypoint following)
- Receives commands via Serial (115200 baud)
- Sends status response via Serial

### ESP32S3 (`/ESP32S3/ESP32S3.ino`)
- WiFi connectivity (station mode)
- Web server on port 80
- Serves HTML control page
- JSON status API (`/api/status`)
- Control endpoint (`/control?cmd=<command>`)
- Communicates with RA4M1 via Serial

### Inter-Core Protocol
- Baud: 115200
- Commands: Single-byte codes (0x01-0x09)
- Status: `#`-delimited string (LAT#LNG#SPD#HDG#GPS_OK#COMP_OK#WP_COUNT#)

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
- Serial (RA4M1<->ESP32S3): 115200 baud

## Preferences
- Always use Arduino IDE and Arduino coding conventions
- Always use GitHub to mirror projects

## Status
In progress — split architecture implemented, needs testing
