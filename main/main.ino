#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include "Sensors.h"


#include "InjectionControl.h"
#include "WebHandler.h"
#include "PreferencesManager.h"
#include "TempControl.h"

// Reference the firmware version defined in WebHandler.cpp
extern const char* firmwareVersion;
extern String tzName;  // from PreferencesManager

// WiFi credentials - replace with your network SSID and password
const char* ssid = "Toucan Dream 1";
const char* password = "Poolisnext22";

// Static IP configuration - update these values according to your network
IPAddress local_IP(192, 168, 1, 90); // Use different IP than temp controller
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// Time server settings
const char* ntpServer = "pool.ntp.org";

// Variables for WiFi reconnection logic
unsigned long previousMillis = 0;
const long wifiCheckInterval = 30000; // Check WiFi every 30 seconds

// Create web server
WebServer server(80);

// Setup function
void setup() {
  // Start serial communication
  Serial.begin(115200);
  Serial.println("\nStarting Combined Pool Control System");
  
  // Load persistent settings
  loadSettings();
  
  // Initialize temperature control system
  initTempControl();
  
  // Initialize sensors
  initSensors();
  
  // Initialize injection control
  initInjectionControl();
  
  // Test relay pin directly (temporary code for troubleshooting)
  int relayTestPin = 5;  // This is the pin number for the relay
  pinMode(relayTestPin, OUTPUT);
  digitalWrite(relayTestPin, HIGH);  // Set to HIGH to test
  Serial.println("Forcing relay pin 5 HIGH for testing");
  delay(1000);
  Serial.print("After force, relay pin 5 state: ");
  Serial.println(digitalRead(relayTestPin));
  
  // Configure and connect to WiFi
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  
  // Configure time
// Sync NTP as UTC, then apply our TZ/DST rules
configTime(0, 0, ntpServer);
setenv("TZ", tzName.c_str(), 1);
tzset();
Serial.printf("Time zone set to %s\n", tzName.c_str());

  
  // Setup web server routes for both systems
  setupWebHandlers(server);
  
  // Start the web server
  server.begin();
  Serial.println("Web server started");
}

// Main loop
void loop() {
  // Handle client connections
  server.handleClient();
  
  // Update sensor readings
  updateSensors();
  
  // Update CO2 injection control
  updateInjectionControl();
  
  // Update temperature control
  updateTempControl();
  
  // Check and maintain WiFi connection
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= wifiCheckInterval) {
    previousMillis = currentMillis;
    
    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost! Reconnecting...");
      
      // First try to configure static IP
      if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        Serial.println("STA Failed to configure static IP");
      }
      
      WiFi.begin(ssid, password);
      
      // Wait up to 10 seconds for connection
      int retryCount = 0;
      while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
        delay(500);
        Serial.print(".");
        retryCount++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.print("Reconnected! IP address: ");
        Serial.println(WiFi.localIP());
      } else {
        Serial.println();
        Serial.println("Failed to reconnect. Will try again later.");
      }
    }
  }
  
  // Small delay to ease CPU load
  delay(50);
}