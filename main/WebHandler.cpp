#include "WebHandler.h"
#include <WiFi.h>
#include <time.h>
#include <Update.h>
#include <WebServer.h>
#include <stdlib.h>      // for setenv(), tzset()


#include "PreferencesManager.h"
#include "Sensors.h"
#include "InjectionControl.h"
#include "TempControl.h"

// Forward declaration: define handleTrend() in this file.
void handleTrend();
void handleUpdate();
void handleDoUpdate();

// Define firmware version directly in WebHandler.cpp
const char* firmwareVersion = "1.7.2"; // Hardcoded version

// Define a reasonable default size for firmware updates instead of using external variable
const size_t FIRMWARE_MAX_SIZE = 1024 * 1024; // 1MB should be sufficient for most ESP32 firmware

// Declare external variable from Sensors.cpp.
extern int numReadings;
extern float dailyAvg[10];
extern int currentDay;

// Global calibration and setting variables for pH/CO2 system
float calibrationOffset = 0.0;
float calibrationSlope = 0.0;
float targetPH = 7.4;
float desiredCO2PerPulse_lbs = 1.2;
unsigned long manualPulseLength = 60000;
unsigned long pulseInterval = 60UL * 60000UL;
int desiredPulseCount = 5;
int scheduledHour1 = 9;
int scheduledMinute1 = 0;
int scheduledHour2 = 13;
int scheduledMinute2 = 0;
int pulsesRemaining = 0; // Variable to track remaining pulses

// External variables from TempControl.cpp
extern float thresholdTemperature;
extern float tempCalibrationOffset;  // Changed from calibrationOffset
extern float voltageDividerResistor;
extern bool useFahrenheit;
extern bool shutdownLatch;
extern String tzName;   // <<— ADD this line

// Pointer to the global web server.
WebServer *webServer;

// Helper: converts 24-hour time to 12-hour format string.
String formatTime(int hour24, int minute) {
  int displayHour = hour24 % 12;
  if (displayHour == 0) displayHour = 12;
  String ampm = (hour24 < 12) ? "AM" : "PM";
  String minuteStr = (minute < 10) ? "0" + String(minute) : String(minute);
  return String(displayHour) + ":" + minuteStr + " " + ampm;
}

// Replace handleTrend with this complete new implementation
void handleTrend() {
  String page = "<html><head><meta charset='UTF-8'><title>pH Trend</title>";
  page += "<style>body { font-family: Arial, sans-serif; background: #f8f8f8; margin: 20px; }";
  page += ".container { max-width: 600px; margin: auto; background: #fff; padding: 20px; border: 1px solid #ccc; }";
  page += "table { width: 100%; border-collapse: collapse; }";
  page += "th, td { border: 1px solid #ccc; padding: 8px; text-align: center; }";
  page += "th { background: #ddd; }";
  page += ".chart { width: 100%; height: 300px; margin-top: 20px; position: relative; }";
  page += ".bar { background: #4a6da7; margin-right: 2px; display: inline-block; }";
  page += ".chart-labels { display: flex; justify-content: space-between; margin-top: 5px; }";
  page += ".chart-label { font-size: 0.8em; color: #666; }";
  page += ".info-box { background: #f8f8f8; border: 1px solid #ddd; padding: 10px; margin: 10px 0; border-radius: 4px; }";
  page += "</style></head><body><div class='container'>";
  page += "<h1>pH Trend (Past 10 Days)</h1>";
  
  // Add information box to explain the display
  page += "<div class='info-box'>";
  page += "<p><strong>Reading Guide:</strong></p>";
  page += "<p>• Table: Most recent day (Day 10) appears at the top</p>";
  page += "<p>• Chart: Most recent day appears on the right side</p>";
  page += "</div>";
  
  // Debug information to help troubleshoot
  page += "<p><small>Current Day Index: " + String(currentDay) + "</small></p>";
  
  // Display table of daily averages
  page += "<table><tr><th>Day</th><th>Average pH</th><th>Age</th></tr>";
  
  // Find min and max for scaling the chart
  float minVal = 14.0;
  float maxVal = 0.0;
  for (int i = 0; i < 10; i++) {
    if (dailyAvg[i] > 0) { // Only consider valid readings
      minVal = min(minVal, dailyAvg[i]);
      maxVal = max(maxVal, dailyAvg[i]);
    }
  }
  
  // Ensure we have a valid range
  if (minVal >= maxVal) {
    minVal = 6.0;
    maxVal = 8.0;
  }
  
  // Add some padding to the range
  float range = maxVal - minVal;
  minVal = max(0.0f, minVal - range * 0.1f);
  maxVal = min(14.0f, maxVal + range * 0.1f);
  
  // Display the daily average values from newest (index 9) to oldest (index 0)
  for (int i = 9; i >= 0; i--) {
    int dayNumber = 10 - (9 - i);
    int daysAgo = 9 - i;
    
    page += "<tr><td>" + String(dayNumber) + "</td>";
    
    if (dailyAvg[i] > 0) {
      page += "<td>" + String(dailyAvg[i], 2) + "</td>";
      page += "<td>" + (daysAgo == 0 ? "Today" : (daysAgo == 1 ? "Yesterday" : String(daysAgo) + " days ago")) + "</td></tr>";
    } else {
      page += "<td>No data</td><td>" + (daysAgo == 0 ? "Today" : (daysAgo == 1 ? "Yesterday" : String(daysAgo) + " days ago")) + "</td></tr>";
    }
  }
  page += "</table>";
  
  // Add a simple bar chart visualization with clear labels
  page += "<h2>Visual Representation</h2>";
  page += "<div class='chart'>";
  for (int i = 0; i <= 9; i++) {  // Changed to go from oldest to newest (left to right)
    if (dailyAvg[i] > 0) {
      // Calculate bar height percentage based on the value range
      int heightPercentage = (int)(((dailyAvg[i] - minVal) / (maxVal - minVal)) * 100);
      
      // Ensure the bar is visible even for small values
      heightPercentage = max(5, heightPercentage);
      
      page += "<div class='bar' style='height: " + String(heightPercentage) + "%; width: 8%;' title='Day " + 
              String(i + 1) + ": " + String(dailyAvg[i], 2) + "'></div>";
    } else {
      // Empty placeholder for missing data
      page += "<div class='bar' style='height: 0%; width: 8%;' title='No data'></div>";
    }
  }
  page += "</div>";
  
  // Add chart axis labels
  page += "<div class='chart-labels'>";
  page += "<div class='chart-label'>Oldest (Day 1)</div>";
  page += "<div class='chart-label'>Most Recent (Day 10)</div>";
  page += "</div>";
  
  // Add value range information
  page += "<p style='text-align: center; font-size: 0.8em;'>pH Range: " + 
           String(minVal, 2) + " to " + String(maxVal, 2) + "</p>";
  
  page += "<p><a href='/'>Return to Main Page</a></p></div></body></html>";
  webServer->send(200, "text/html", page);
}

// Handler for firmware update page
void handleUpdate() {
  String page = "<html><head><meta charset='UTF-8'><title>Firmware Update</title>";
  page += "<style>";
  page += "body { background: #ececec; font-family: Arial, sans-serif; margin: 0; padding: 20px; }";
  page += ".container { background: #fff; padding: 20px 40px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.2); max-width: 600px; margin: auto; }";
  page += "h1 { text-align: center; }";
  page += "p { font-size: 1.2em; margin: 10px 0; }";
  page += "form { margin: 15px 0; }";
  page += "input[type='file'] { margin: 10px 0; }";
  page += "input[type='submit'] { padding: 10px 20px; border: none; background-color: #333; color: #fff; border-radius: 4px; cursor: pointer; }";
  page += "input[type='submit']:hover { background-color: #555; }";
  page += "</style></head><body>";
  page += "<div class='container'>";
  page += "<h1>Firmware Update</h1>";
  page += "<p><strong>Current Firmware Version:</strong> " + String(firmwareVersion) + "</p>";
  page += "<form method='POST' action='/doUpdate' enctype='multipart/form-data'>";
  page += "<p>Select firmware file to upload (.bin):</p>";
  page += "<input type='file' name='update' accept='.bin'>";
  page += "<input type='submit' value='Update Firmware'>";
  page += "</form>";
  page += "<p><a href='/'>Return to Main Page</a></p>";
  page += "</div></body></html>";
  webServer->send(200, "text/html", page);
}

// Handler for processing firmware update
void handleDoUpdate() {
  HTTPUpload& upload = webServer->upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Update: %s\n", upload.filename.c_str());
    if (!Update.begin(FIRMWARE_MAX_SIZE)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
  
  yield();
}

// Handler for update complete page
void handleUpdateDone() {
  String page = "<html><head><meta charset='UTF-8'><title>Update Complete</title>";
  page += "<style>body { font-family: Arial, sans-serif; background: #f8f8f8; margin: 20px; }";
  page += ".container { max-width: 600px; margin: auto; background: #fff; padding: 20px; border: 1px solid #ccc; }";
  page += "h1 { text-align: center; color: green; }</style>";
  page += "<script>setTimeout(function(){ window.location.href = '/'; }, 5000);</script>";
  page += "</head><body><div class='container'>";
  page += "<h1>Update Successful!</h1>";
  page += "<p>Your device will restart shortly.</p>";
  page += "</div></body></html>";
  webServer->send(200, "text/html", page);
  delay(1000);
  ESP.restart();
}

// Handler for the main page.
void handleRoot() {
  char timeStr[20];
  time_t now = time(nullptr);
  if (now < 8 * 3600) {
    strcpy(timeStr, "Time not set");
  } else {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%I:%M:%S %p", &timeinfo);
  }
  String page;

  float currentPH = readPH();
  float runningPH = getRunningPH();
  float deltaP = computeChokeDifferentialPressure();
  float deltaP_kPa = deltaP / 1000.0;
  float deltaP_psi = deltaP / 6894.76;

  // Use the computeMassFlowRate function instead of a placeholder
  float massFlowRate = computeMassFlowRate();  // kg/s

  // Convert desiredCO2PerPulse_lbs to kg and divide by mass flow rate in kg/s to get seconds
  float calculatedPulseTime_sec = (desiredCO2PerPulse_lbs * 0.45359237) / massFlowRate;

  // Add a safety check to prevent extremely long valve open times
  if (massFlowRate < 0.0001 || calculatedPulseTime_sec > 300.0) {
    calculatedPulseTime_sec = 300.0;
  }
  
  float pulseIntervalMin = pulseInterval / 60000.0;
  
page = "<html><head><meta charset='UTF-8'><title>Pool CO2/pH & Pump Temp S/D</title>";


  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<meta http-equiv='Cache-Control' content='no-cache, no-store, must-revalidate'>";
  page += "<meta http-equiv='Pragma' content='no-cache'>";
  page += "<meta http-equiv='Expires' content='0'>";
  page += "<meta http-equiv='refresh' content='6'>"; // Refresh every 6 seconds

  // Add JavaScript for auto-refresh functionality
  
  page += "<style>";
  page += "* { box-sizing: border-box; }";
  page += "body { font-family: Arial, sans-serif; margin: 0; padding: 0; background: #f0f0f0; }";
  page += ".page-container { max-width: 1000px; margin: 20px auto; display: flex; flex-wrap: wrap; gap: 20px; }";
  
  // Navigation styling
  page += ".nav-links { display: flex; justify-content: center; background: #333; padding: 10px 0; margin-bottom: 20px; }";
  page += ".nav-links a { color: white; text-decoration: none; padding: 10px 20px; margin: 0 5px; border-radius: 4px; }";
  page += ".nav-links a.active { background: #4a6da7; }";
  page += ".nav-links a:hover { background: #555; }";
  
  // Section-specific styling
  page += ".section { border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); padding: 20px; margin-bottom: 10px; }";
  page += ".section-header { text-align: center; border-bottom: 2px solid rgba(0,0,0,0.1); padding-bottom: 10px; margin-top: 0; }";
  
  // Different background colors for each section
  page += ".section-info { background: #e8f4f8; width: 100%; }";  // Light blue
  page += ".section-settings { background: #f0f8e8; width: 100%; }";  // Light green
  page += ".section-actions { background: #f8e8e8; width: 100%; }";  // Light pink
  page += ".section-maintenance { background: #f0e8f8; width: 100%; }";  // Light purple
  
  // Form and input styling
  page += "form { margin: 15px 0; display: flex; align-items: center; flex-wrap: wrap; gap: 10px; }";
  page += "label { font-weight: bold; }";
  page += "input[type='text'] { padding: 8px; border: 1px solid #ccc; border-radius: 4px; width: 80px; }";
  page += "button { padding: 8px 15px; border: none; background-color: #4a6da7; color: #fff; border-radius: 4px; cursor: pointer; }";
  page += "button:hover { background-color: #36508c; }";
  
  // Table styling for data display
  page += "table { width: 100%; border-collapse: collapse; margin: 15px 0; }";
  page += "th, td { padding: 8px; text-align: left; border-bottom: 1px solid rgba(0,0,0,0.1); }";
  
  // Responsive design
  page += "@media (min-width: 768px) {";
  page += "  .section-info, .section-settings, .section-actions, .section-maintenance { width: calc(50% - 10px); }";
  page += "}";
  
  page += ".version { font-size: 0.8em; color: #666; text-align: center; margin-top: 20px; }";
  page += ".trend-button { display: inline-block; margin-top: 15px; text-decoration: none; }";
  page += ".trend-button button { width: 200px; padding: 10px; }";
  page += ".refresh-timestamp { font-size: 0.8em; color: #666; text-align: right; margin-top: 5px; }";
  page += "</style></head><body>";
  
  // Navigation bar
  page += "<div class='nav-links'>";
  page += "<a href='/' class='active'>pH & CO2 Control</a>";
  page += "<a href='/temperature'>Temperature Control</a>";
  page += "</div>";
  
  page += "<div class='page-container'>";
  
  // Section 1: Manual Control Info
  page += "<div class='section section-info'>";
  page += "<h2 class='section-header'>System Status</h2>";
  // Add a small timestamp to show when the page was last refreshed
  page += "<div class='refresh-timestamp' id='refresh-timestamp'>Last refresh: ";
  page += String(timeStr);
  page += "</div>";
  page += "<table>";
  page += "<tr><td><strong>Current pH:</strong></td><td>" + String(currentPH, 2) + "</td></tr>";
  page += "<tr><td><strong>Running pH (last " + String(numReadings) + " readings):</strong></td><td>" + String(runningPH, 2) + "</td></tr>";
  page += "<tr><td><strong>Calculated Valve Open Time:</strong></td><td>" + String(calculatedPulseTime_sec, 2) + " seconds</td></tr>";
  page += "<tr><td><strong>Pulse Interval:</strong></td><td>" + String(pulseIntervalMin, 2) + " minutes</td></tr>";
  page += "<tr><td><strong>Number of Pulses Remaining:</strong></td><td>" + String(pulsesRemaining) + "</td></tr>";
  page += "<tr><td><strong>Choked Flow Differential Pressure:</strong></td><td>" + String(deltaP_kPa, 1) + " kPa (" + String(deltaP_psi, 1) + " psi)</td></tr>";
  page += "<tr><td><strong>Current Time (UTC-5):</strong></td><td>" + String(timeStr) + "</td></tr>";
  page += "</table>";
  page += "<div style='text-align: center;'>";
  page += "<a href='/trend' class='trend-button'><button>View 10-Day pH Trend</button></a>";
  page += "</div>";
  page += "</div>";
  
  // Section 2: Scheduled Injection Times and Calibration Constants
  page += "<div class='section section-settings'>";
  page += "<h2 class='section-header'>System Settings</h2>";
  
  page += "<h3>Scheduled Injection Times</h3>";
  page += "<p><strong>Time 1:</strong> " + formatTime(scheduledHour1, scheduledMinute1) + "</p>";
  page += "<form action='/setSchedule1' method='GET'>";
  page += "<label>Hour:</label> <input type='text' name='hour' value='" + String(scheduledHour1 % 12 == 0 ? 12 : scheduledHour1 % 12) + "'>";
  page += "<label>Minute:</label> <input type='text' name='minute' value='" + String(scheduledMinute1) + "'>";
  page += "<label>AM/PM:</label> <input type='text' name='ampm' value='" + String(scheduledHour1 < 12 ? "AM" : "PM") + "'>";
  page += "<button type='submit'>Set Time 1</button></form>";
  
  page += "<p><strong>Time 2:</strong> " + formatTime(scheduledHour2, scheduledMinute2) + "</p>";
  page += "<form action='/setSchedule2' method='GET'>";
  page += "<label>Hour:</label> <input type='text' name='hour' value='" + String(scheduledHour2 % 12 == 0 ? 12 : scheduledHour2 % 12) + "'>";
  page += "<label>Minute:</label> <input type='text' name='minute' value='" + String(scheduledMinute2) + "'>";
  page += "<label>AM/PM:</label> <input type='text' name='ampm' value='" + String(scheduledHour2 < 12 ? "AM" : "PM") + "'>";
  page += "<button type='submit'>Set Time 2</button></form>";
  
  page += "<h3>Calibration Settings</h3>";
  page += "<form action='/setNumReadings' method='GET'>";
  page += "<label>Number of pH Readings:</label> <input type='text' name='numReadings' value='" + String(numReadings) + "'>";
  page += "<button type='submit'>Set</button></form>";
  
  page += "<form action='/setCalOffset' method='GET'>";
  page += "<label>Calibration Offset (±x.x):</label> <input type='text' name='offset' value='" + String(calibrationOffset, 1) + "'>";
  page += "<button type='submit'>Set</button></form>";
  
  page += "<form action='/setCalSlope' method='GET'>";
  page += "<label>Calibration Slope (x.x):</label> <input type='text' name='slope' value='" + String(calibrationSlope, 1) + "'>";
  page += "<button type='submit'>Set</button></form>";
  page += "</div>";
  
  // Section 3: Manual Control Actions
  page += "<div class='section section-actions'>";
  page += "<h2 class='section-header'>Control Actions</h2>";
  page += "<form action='/toggleInjection' method='GET'><button type='submit' style='width: 100%; padding: 12px; margin-bottom: 15px; font-size: 16px; background-color: #4CAF50;'>Start New Injection Series</button></form>";
  
  page += "<h3>Injection Parameters</h3>";
  page += "<form action='/setCO2' method='GET'>";
  page += "<label>Desired CO₂ per Pulse (lbs):</label> <input type='text' name='co2' value='" + String(desiredCO2PerPulse_lbs, 2) + "'>";
  page += "<button type='submit'>Set</button></form>";
  
  page += "<form action='/setpH' method='GET'>";
  page += "<label>Target pH:</label> <input type='text' name='target' value='" + String(targetPH, 2) + "'>";
  page += "<button type='submit'>Set</button></form>";
  
  page += "<form action='/setPulseInterval' method='GET'>";
  page += "<label>Pulse Interval (min):</label> <input type='text' name='pulseInterval' value='" + String(pulseIntervalMin, 2) + "'>";
  page += "<button type='submit'>Set</button></form>";
  
  page += "<form action='/setPulseCount' method='GET'>";
  page += "<label>Number of Pulses:</label> <input type='text' name='pulseCount' value='" + String(desiredPulseCount) + "'>";
  page += "<button type='submit'>Set</button></form>";
  
  page += "<form action='/setPulseLength' method='GET'>";
  page += "<label>Pulse Duration (sec):</label> <input type='text' name='pulse' value='" + String(manualPulseLength / 1000.0, 1) + "'>";
  page += "<button type='submit'>Set</button></form>";
  page += "</div>";
  
  // Section 4: Firmware Update and Links
  page += "<div class='section section-maintenance'>";
  page += "<h2 class='section-header'>System Maintenance</h2>";
  page += "<div style='display: flex; justify-content: space-around; margin: 20px 0;'>";
  page += "<a href='/update' style='text-decoration: none;'><button style='width: 200px; padding: 10px;'>Firmware Update</button></a>";
  page += "</div>";
  page += "<p class='version'>Firmware Version: " + String(firmwareVersion) + "</p>";
  page += "</div>";
  
  page += "</div>"; // Close page-container

   page += "<tr><td><strong>Current Time (UTC-5):</strong></td><td>" + String(timeStr) + "</td></tr>";
   page += "</table>";

  // ── Time‑zone selector form ─────────────────
  page += "<hr><form action='/setTimezone' method='GET'>\n";
  page += "  <label>Time Zone:</label>\n";
  page += "  <select name='tz'>\n";

page += "<option value='UTC0'" + String(tzName == "UTC0" ? " selected" : "") + ">UTC</option>";
page += "<option value='EST5EDT,M3.2.0/2,M11.1.0/2'" + String(tzName == "EST5EDT,M3.2.0/2,M11.1.0/2" ? " selected" : "") + ">Eastern</option>";
page += "<option value='CST6CDT,M3.2.0/2,M11.1.0/2'" + String(tzName == "CST6CDT,M3.2.0/2,M11.1.0/2" ? " selected" : "") + ">Central</option>";
page += "<option value='MST7MDT,M3.2.0/2,M11.1.0/2'" + String(tzName == "MST7MDT,M3.2.0/2,M11.1.0/2" ? " selected" : "") + ">Mountain</option>";
page += "<option value='PST8PDT,M3.2.0/2,M11.1.0/2'" + String(tzName == "PST8PDT,M3.2.0/2,M11.1.0/2" ? " selected" : "") + ">Pacific</option>";
page += "<option value='AKST9AKDT,M3.2.0/2,M11.1.0/2'" + String(tzName == "AKST9AKDT,M3.2.0/2,M11.1.0/2" ? " selected" : "") + ">Alaska</option>";
page += "<option value='HST10'" + String(tzName == "HST10" ? " selected" : "") + ">Hawaii</option>";
page += "<option value='MST7'" + String(tzName == "MST7" ? " selected" : "") + ">Arizona (no DST)</option>";


  // (add more zones here in the same pattern)

  page += "  </select>\n";
  page += "  <button type='submit'>Set</button>\n";
  page += "</form>\n";

   // Closing HTML
  page += "</div></body></html>";
  webServer->send(200, "text/html", page);


   // …then the very last lines:
   page += "</div></body></html>";
   webServer->send(200, "text/html", page);
}


// Handler for the temperature control page
void handleTemperatureControl() {
  float currentTemp = getDisplayTemperature();
  Serial.print("Web display temperature: ");
  Serial.println(currentTemp);

  float displayThreshold = getDisplayThreshold();
  
  // Get calibration offset in display units
  float displayCalOffset = tempCalibrationOffset;
  if (useFahrenheit) {
    displayCalOffset = tempCalibrationOffset * 9.0 / 5.0;
  }
  
  String unitSymbol = useFahrenheit ? "&deg;F" : "&deg;C";
  
  String page = "<html><head><meta charset='UTF-8'><title>Pool Temperature Control</title>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  
  // Add cache control and improved refresh mechanism
  page += "<meta http-equiv='Cache-Control' content='no-cache, no-store, must-revalidate'>";
  page += "<meta http-equiv='Pragma' content='no-cache'>";
  page += "<meta http-equiv='Expires' content='0'>";
  page += "<meta http-equiv='refresh' content='6'>"; // Refresh every 6 seconds
  page += "<script>";
  page += "<script>";
page += "  window.onload = function() {";
page += "    // Nothing special needed here for now";
page += "  };";
page += "</script>";
  
  page += "<style>";
  page += "* { box-sizing: border-box; }";
  page += "body { font-family: Arial, sans-serif; margin: 0; padding: 0; background: #f0f0f0; }";
  page += ".page-container { max-width: 1000px; margin: 20px auto; display: flex; flex-wrap: wrap; gap: 20px; }";
  
  // Navigation styling
  page += ".nav-links { display: flex; justify-content: center; background: #333; padding: 10px 0; margin-bottom: 20px; }";
  page += ".nav-links a { color: white; text-decoration: none; padding: 10px 20px; margin: 0 5px; border-radius: 4px; }";
  page += ".nav-links a.active { background: #4a6da7; }";
  page += ".nav-links a:hover { background: #555; }";
  
  // Section styling
  page += ".section { border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); padding: 20px; margin-bottom: 10px; }";
  page += ".section-header { text-align: center; border-bottom: 2px solid rgba(0,0,0,0.1); padding-bottom: 10px; margin-top: 0; }";
  page += ".section-status { background: #e8f4f8; width: 100%; }";
  page += ".section-settings { background: #f0f8e8; width: 100%; }";
  
  // Form and input styling
  page += "form { margin: 15px 0; display: flex; align-items: center; flex-wrap: wrap; gap: 10px; }";
  page += "label { font-weight: bold; }";
  page += "input[type='text'], input[type='number'] { padding: 8px; border: 1px solid #ccc; border-radius: 4px; width: 80px; }";
  page += "button { padding: 8px 15px; border: none; background-color: #4a6da7; color: #fff; border-radius: 4px; cursor: pointer; }";
  page += "button:hover { background-color: #36508c; }";
  page += ".alert { padding: 10px; border-radius: 4px; margin: 10px 0; font-weight: bold; }";
  page += ".alert-danger { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }";
  
  // Responsive design
  page += "@media (min-width: 768px) {";
  page += "  .section-status, .section-settings { width: calc(50% - 10px); }";
  page += "}";
  
  page += "</style></head><body>";
  
  // Navigation bar
  page += "<div class='nav-links'>";
  page += "<a href='/'>pH & CO2 Control</a>";
  page += "<a href='/temperature' class='active'>Temperature Control</a>";
  page += "</div>";
  
  page += "<div class='page-container'>";
  
  // Section 1: Current Status
  page += "<div class='section section-status'>";
  page += "<h2 class='section-header'>Temperature Status</h2>";
  
  // High temp streak warning (3-second window)
  if (isHighTempStreakActive()) {
    unsigned long streakStart = getHighTempEventStartTime();
    char streakTimeStr[20] = "Time not set";
    if (streakStart > 8 * 3600) {
      struct tm timeinfo;
      localtime_r((time_t*)&streakStart, &timeinfo);
      strftime(streakTimeStr, sizeof(streakTimeStr), "%I:%M:%S %p", &timeinfo);
    }
    page += "<div class='alert alert-danger' style='font-size:1.2em;'>";
    page += "<strong>Warning:</strong> High temperature detected!<br>";
    page += "If this continues for 3 seconds, the pump will be shut down.<br>";
    page += "<span style='font-size:0.95em;'>Started at: ";
    page += String(streakTimeStr);
    page += "</span></div>";
  }

  // --- High Temperature Event Log Box ---
  int eventCount = getHighTempEventCount();
  if (eventCount > 0) {
    page += "<div style='background:#fffbe6;border:2px solid #f5c542;padding:12px 16px;margin:16px 0 10px 0;border-radius:7px;max-width:420px;'>";
    page += "<b>Recent High Temperature Events</b><br><ul style='padding-left:18px;margin:10px 0 0 0;'>";
    for (int i = 0; i < eventCount; ++i) {
      float temp;
      time_t t;
      if (getHighTempEvent(i, &temp, &t)) {
        float htmlTemp = temp;
        String htmlUnit = "&deg;C";
        if (useFahrenheit) {
          htmlTemp = temp * 9.0 / 5.0 + 32.0;
          htmlUnit = "&deg;F";
        }
        char timeStr[32] = "";
        if (t > 1577836800) { // Jan 1, 2020 UTC
          struct tm timeinfo;
          localtime_r(&t, &timeinfo);
          strftime(timeStr, sizeof(timeStr), "%b %d, %I:%M:%S %p", &timeinfo);
        } else {
          strncpy(timeStr, "N/A", sizeof(timeStr));
        }
        page += "<li style='margin-bottom:2px;'>";
        page += String(htmlTemp, 2) + " " + htmlUnit + " at " + String(timeStr) + "</li>";
      }
    }
    page += "</ul></div>";
  }

  // Add a timestamp to show when the page was generated
  time_t now = time(nullptr);
  char timeStr[20] = "Time not set";
  if (now > 8 * 3600) {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  }
  page += "<p><small>Last updated: " + String(timeStr) + "</small></p>";
  
  // Make temperature display more prominent
  page += "<p style='font-size: 1.2em;'><strong>Current Temperature:</strong> <span style='font-size: 1.5em; color: #4a6da7;'>" + String(currentTemp, 2) + " " + unitSymbol + "</span></p>";
  
  // Temperature reading debug info (avoiding direct sensorPin access)
  page += "<p><small>Debug Info: Raw temperature = " + String(getTemperatureCelsius()) + "°C</small></p>";
  
  // Display pump status message based on shutdownLatch and temperature
  if (isShutdownLatched()) {
    if (getTemperatureCelsius() >= thresholdTemperature) {
      page += "<div class='alert alert-danger'>";
      page += "High Temperature Shutdown Active. Pump is OFF until manual reset.";
      page += "</div>";
    } else {
      page += "<div class='alert alert-danger'>";
      page += "Pump is OFF. Manual reset required to enable pump.";
      page += "</div>";
    }
  } else {
    page += "<p><strong>Pump Status:</strong> " + String(isPumpShutoff() ? "OFF" : "ON") + "</p>";
  }
  
  page += "<p><strong>Threshold Temperature:</strong> " + String(displayThreshold, 2) + " " + unitSymbol + "</p>";
  
  // Add manual refresh button
  page += "<form action='/temperature' method='GET'>";
  page += "<button type='submit' style='background-color: #4CAF50;'>Refresh Now</button>";
  page += "</form>";
  
  // Add reset button if shutdown is latched
  if (isShutdownLatched()) {
    page += "<form action='/resetpump' method='GET'>";
    page += "<button type='submit' style='background-color: #28a745;'>Reset Pump Shutdown</button>";
    page += "</form>";
  }
  
  page += "</div>";
  
  // Section 2: Temperature Settings
  page += "<div class='section section-settings'>";
  page += "<h2 class='section-header'>Temperature Settings</h2>";
  
  // Form to update threshold temperature
  page += "<form action='/setthreshold' method='GET'>";
  page += "<label>Threshold Temperature (" + unitSymbol + "):</label>";
  page += "<input type='number' step='0.1' name='threshold' value='" + String(displayThreshold, 2) + "'>";
  page += "<button type='submit'>Update</button>";
  page += "</form>";
  
  // Form to update calibration offset
  page += "<form action='/settempcal' method='GET'>";
  page += "<label>Calibration Offset (" + unitSymbol + "):</label>";
  page += "<input type='number' step='0.1' name='offset' value='" + String(displayCalOffset, 2) + "'>";
  page += "<button type='submit'>Update</button>";
  page += "</form>";
  
  // Form to update voltage divider resistor value
  page += "<form action='/setresistor' method='GET'>";
  page += "<label>Voltage Divider Resistor (ohm):</label>";
  page += "<input type='number' step='1' name='resistor' value='" + String(voltageDividerResistor, 0) + "'>";
  page += "<button type='submit'>Update</button>";
  page += "</form>";
  
  // Form to update supply voltage - NEW ADDITION
  page += "<form action='/setVoltage' method='GET'>";
  page += "<label>Supply Voltage (V):</label>";
  page += "<input type='number' step='0.1' min='1.0' max='5.5' name='voltage' value='" + String(getSupplyVoltage(), 1) + "'>";
  page += "<button type='submit'>Update</button>";
  page += "</form>";
  
  // Link to toggle temperature unit
  page += "<p><strong>Current Unit:</strong> " + unitSymbol + "</p>";
  if (useFahrenheit) {
    page += "<a href='/setunit?unit=C'><button>Switch to Celsius</button></a>";
  } else {
    page += "<a href='/setunit?unit=F'><button>Switch to Fahrenheit</button></a>";
  }
  
  page += "</div>";
  
  page += "</div>"; // Close page-container
  page += "</body></html>";
  
  webServer->send(200, "text/html", page);
  saveHighTempEvents();
}

// Handler to update the threshold temperature
void handleSetThreshold() {
  if (webServer->hasArg("threshold")) {
    float newThreshold = webServer->arg("threshold").toFloat();
    setThresholdTemperature(newThreshold);
  }
  webServer->sendHeader("Location", "/temperature");
  webServer->send(303, "text/plain", "Threshold updated");
}

// Handler to update the calibration offset
void handleSetTempCalOffset() {
  if (webServer->hasArg("offset")) {
    float newOffset = webServer->arg("offset").toFloat();
    setCalibrationOffset(newOffset);
  }
  webServer->sendHeader("Location", "/temperature");
  webServer->send(303, "text/plain", "Calibration offset updated");
}

// Handler to update the voltage divider resistor value
void handleSetDividerResistor() {
  if (webServer->hasArg("resistor")) {
    float newResistance = webServer->arg("resistor").toFloat();
    setDividerResistor(newResistance);
  }
  webServer->sendHeader("Location", "/temperature");
  webServer->send(303, "text/plain", "Voltage divider resistor updated");
}

// NEW HANDLER for setting supply voltage
void handleSetVoltage() {
  if (webServer->hasArg("voltage")) {
    float newVoltage = webServer->arg("voltage").toFloat();
    setSupplyVoltage(newVoltage);
  }
  webServer->sendHeader("Location", "/temperature");
  webServer->send(303, "text/plain", "Supply voltage updated");
}

// Handler to update the temperature display unit
void handleSetTempUnit() {
  if (webServer->hasArg("unit")) {
    String unit = webServer->arg("unit");
    toggleTemperatureUnit(unit == "F");
  }
  webServer->sendHeader("Location", "/temperature");
  webServer->send(303, "text/plain", "Unit updated");
}

// Handler to reset the pump shutdown latch
void handleResetPumpShutdown() {
  resetShutdownLatch();
  webServer->sendHeader("Location", "/temperature");
  webServer->send(303, "text/plain", "Pump shutdown reset");
}

// Handler for setting schedule 1
void handleSetSchedule1() {
  if (webServer->hasArg("hour") && webServer->hasArg("minute") && webServer->hasArg("ampm")) {
    int hour = webServer->arg("hour").toInt();
    int minute = webServer->arg("minute").toInt();
    String ampm = webServer->arg("ampm");
    
    // Convert from 12-hour to 24-hour format
    if (ampm.equalsIgnoreCase("PM") && hour < 12) {
      hour += 12;
    } else if (ampm.equalsIgnoreCase("AM") && hour == 12) {
      hour = 0;
    }
    
    // Validate input
    if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
      scheduledHour1 = hour;
      scheduledMinute1 = minute;
      saveInt("schHour1", hour);
      saveInt("schMin1", minute);
    }
  }
  webServer->sendHeader("Location", "/");
  webServer->send(302, "text/plain", "");
}

// Handler for setting schedule 2
void handleSetSchedule2() {
  if (webServer->hasArg("hour") && webServer->hasArg("minute") && webServer->hasArg("ampm")) {
    int hour = webServer->arg("hour").toInt();
    int minute = webServer->arg("minute").toInt();
    String ampm = webServer->arg("ampm");
    
    // Convert from 12-hour to 24-hour format
    if (ampm.equalsIgnoreCase("PM") && hour < 12) {
      hour += 12;
    } else if (ampm.equalsIgnoreCase("AM") && hour == 12) {
      hour = 0;
    }
    
    // Validate input
    if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
      scheduledHour2 = hour;
      scheduledMinute2 = minute;
      saveInt("schHour2", hour);
      saveInt("schMin2", minute);
    }
  }
  webServer->sendHeader("Location", "/");
  webServer->send(302, "text/plain", "");
}

// Handler for setting number of pH readings for running average
void handleSetNumReadings() {
  if (webServer->hasArg("numReadings")) {
    int readings = webServer->arg("numReadings").toInt();
    if (readings > 0 && readings <= 100) {
      numReadings = readings;
      saveInt("numReadings", numReadings);
      allocatePHBuffer(); // Reallocate buffer with new size
    }
  }
  webServer->sendHeader("Location", "/");
  webServer->send(302, "text/plain", "");
}

// Handler for setting calibration offset
void handleSetCalOffset() {
  if (webServer->hasArg("offset")) {
    float offset = webServer->arg("offset").toFloat();
    calibrationOffset = offset;
    saveFloat("calOffset", calibrationOffset);
  }
  webServer->sendHeader("Location", "/");
  webServer->send(302, "text/plain", "");
}

// Handler for setting calibration slope
void handleSetCalSlope() {
  if (webServer->hasArg("slope")) {
    float slope = webServer->arg("slope").toFloat();
    if (slope != 0) { // Avoid division by zero issues
      calibrationSlope = slope;
      saveFloat("calSlope", calibrationSlope);
    }
  }
  webServer->sendHeader("Location", "/");
  webServer->send(302, "text/plain", "");
}

// Handler for toggling injection
void handleToggleInjection() {
  // Start a new injection series using the InjectionControl module
  startInjectionSeries(desiredPulseCount);
  
  webServer->sendHeader("Location", "/");
  webServer->send(302, "text/plain", "");
}

// Handler for setting CO2 per pulse
void handleSetCO2() {
  if (webServer->hasArg("co2")) {
    float co2 = webServer->arg("co2").toFloat();
    if (co2 > 0) {
      desiredCO2PerPulse_lbs = co2;
      saveFloat("co2PerPulse", desiredCO2PerPulse_lbs);
    }
  }
  webServer->sendHeader("Location", "/");
  webServer->send(302, "text/plain", "");
}

// Handler for setting target pH
void handleSetPH() {
  if (webServer->hasArg("target")) {
    float ph = webServer->arg("target").toFloat();
    if (ph >= 0 && ph <= 14) {
      targetPH = ph;
      saveFloat("targetPH", targetPH);
    }
  }
  webServer->sendHeader("Location", "/");
  webServer->send(302, "text/plain", "");
}

// Handler for setting pulse interval
void handleSetPulseInterval() {
  if (webServer->hasArg("pulseInterval")) {
    float intervalMin = webServer->arg("pulseInterval").toFloat();
    if (intervalMin > 0) {
      pulseInterval = (unsigned long)(intervalMin * 60000UL); // Convert minutes to milliseconds
      saveULong("pulseInterval", pulseInterval);
    }
  }
  webServer->sendHeader("Location", "/");
  webServer->send(302, "text/plain", "");
}

// Handler for setting pulse count
void handleSetPulseCount() {
  if (webServer->hasArg("pulseCount")) {
    int count = webServer->arg("pulseCount").toInt();
    if (count > 0) {
      desiredPulseCount = count;
      saveInt("pulseCount", desiredPulseCount);
    }
  }
  webServer->sendHeader("Location", "/");
  webServer->send(302, "text/plain", "");
}

// Handler for setting pulse length
void handleSetPulseLength() {
  if (webServer->hasArg("pulse")) {
    float pulseSec = webServer->arg("pulse").toFloat();
    if (pulseSec > 0) {
      manualPulseLength = (unsigned long)(pulseSec * 1000UL); // Convert seconds to milliseconds
      saveULong("pulseLength", manualPulseLength);
    }
  }
  webServer->sendHeader("Location", "/");
  webServer->send(302, "text/plain", "");
}

// ── Time‑Zone Change Handler ─────────────────
void handleSetTimezone() {
  if (webServer->hasArg("tz")) {
    String v = webServer->arg("tz");
    tzName = v;                   // update RAM copy
    saveString("tz", v.c_str());  // persist the choice
    setenv("TZ", v.c_str(), 1);   // apply new POSIX TZ
    tzset();                      // rebuild localtime rules
  }
  webServer->sendHeader("Location", "/");
  webServer->send(302, "text/plain", "");
}


// Register all routes
void setupWebHandlers(WebServer &serverRef) {
  webServer = &serverRef;

  // pH/CO₂ routes
  serverRef.on("/",            handleRoot);
  serverRef.on("/trend",       handleTrend);
  serverRef.on("/update",      HTTP_GET,  handleUpdate);
  serverRef.on("/doUpdate",    HTTP_POST,
    []() {
      webServer->sendHeader("Connection","close");
      webServer->send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
      handleUpdateDone();
    },
    []() {
      handleDoUpdate();
    }
  );

  // pH/CO₂ form handlers
  serverRef.on("/setSchedule1",     HTTP_GET, handleSetSchedule1);
  serverRef.on("/setSchedule2",     HTTP_GET, handleSetSchedule2);
  serverRef.on("/setNumReadings",   HTTP_GET, handleSetNumReadings);
  serverRef.on("/setCalOffset",     HTTP_GET, handleSetCalOffset);
  serverRef.on("/setCalSlope",      HTTP_GET, handleSetCalSlope);
  serverRef.on("/toggleInjection",  HTTP_GET, handleToggleInjection);
  serverRef.on("/setCO2",           HTTP_GET, handleSetCO2);
  serverRef.on("/setpH",            HTTP_GET, handleSetPH);
  serverRef.on("/setPulseInterval", HTTP_GET, handleSetPulseInterval);
  serverRef.on("/setPulseCount",    HTTP_GET, handleSetPulseCount);
  serverRef.on("/setPulseLength",   HTTP_GET, handleSetPulseLength);

  // Temperature control routes
  serverRef.on("/temperature",      HTTP_GET, handleTemperatureControl);
  serverRef.on("/setthreshold",     HTTP_GET, handleSetThreshold);
  serverRef.on("/settempcal",       HTTP_GET, handleSetTempCalOffset);
  serverRef.on("/setresistor",      HTTP_GET, handleSetDividerResistor);
  serverRef.on("/setunit",          HTTP_GET, handleSetTempUnit);
  serverRef.on("/resetpump",        HTTP_GET, handleResetPumpShutdown);
  serverRef.on("/setVoltage",       HTTP_GET, handleSetVoltage);

  // Time‑zone route
  serverRef.on("/setTimezone",      HTTP_GET, handleSetTimezone);
}
