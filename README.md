# ESP32 Pool CO2 and Pump Control System

## Overview
This project is an advanced ESP32-based controller for pool temperature and CO2 injection management. It features:
- Real-time temperature monitoring and control
- Automatic pump shutoff on high temperature
- Email alerts for high temperature events (with multiple recipients)
- Web interface for monitoring and configuration
- NTP time synchronization and event logging
- Persistent event storage and robust error handling

## Features
- **Temperature Sensing:** Reads pool temperature using a thermistor and ADC.
- **Pump Control:** Automatically enables/disables pump based on safe temperature thresholds.
- **CO2 Injection:** Controls CO2 injection for pH management (if hardware present).
- **Email Alerts:** Sends alerts to multiple recipients when high temperature is detected.
- **Web Server:** ESP32 hosts a web interface for live monitoring and configuration.
- **Event Logging:** Logs high temperature events with timestamps (NTP synchronized).
- **Manual Reset:** Shutdown latch can be cleared with a button or via the web interface.
- **Persistent Storage:** Settings and events survive power cycles.

## Hardware Requirements
- ESP32 Dev Board
- Thermistor (10K NTC recommended)
- Relay module for pump control
- Optional: CO2 solenoid and pH injection hardware
- Momentary pushbutton for manual reset

## Software Requirements
- Arduino IDE with ESP32 board support
- Libraries:
  - `ESP_Mail_Client`
  - `WiFi.h`, `WebServer.h`, `Preferences.h`
  - Any other libraries specified in the code

## Setup Instructions
1. **Clone this Repository:**
   ```sh
   git clone https://github.com/crblair/pHandPumpControl.git
   ```
2. **Open in Arduino IDE:**
   - Open `main.ino` from the `main` directory.
3. **Configure WiFi and Email:**
   - Edit WiFi SSID/password and email credentials in `main.ino`.
   - Update static IP if needed.
   - Set alert recipient emails as desired.
4. **Connect Hardware:**
   - Wire thermistor, relay, reset button, and optional CO2 hardware to ESP32.
5. **Upload to ESP32:**
   - Select the correct board and port, then upload.
6. **Access Web Interface:**
   - Connect to the ESP32's IP address in your browser.

## Version History

### v1.0.0 (Initial Release)
- Basic temperature monitoring and pump control
- Web interface and event logging

### v1.1.0
- Added email alert system
- Improved NTP synchronization and timestamp handling
- Enhanced error handling and persistent storage

### v1.2.0 (Current)
- **Bugfix:** Fixed Fahrenheit double-conversion in alert emails
- **Feature:** Added support for multiple email recipients
- **Change:** Updated static IP address
- **Stability:** Improved event logging and time sync reliability

## Usage Notes
- The web interface shows live temperature, pump status, and event history.
- Email alerts are sent to all configured recipients when a high temperature event triggers the shutdown latch.
- Manual reset can be performed via button or web interface after a shutdown event.

## Troubleshooting
- Ensure WiFi credentials and static IP are correct for your network.
- For email alerts, use an app password if using Gmail with 2FA.
- Check serial output for debug information during setup and operation.

## License
This project is for personal and educational use. See LICENSE file if present.

---

For questions or contributions, please open an issue or pull request on GitHub.
