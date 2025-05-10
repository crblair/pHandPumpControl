#include "TempControl.h"
#include "PreferencesManager.h"
#include <Preferences.h>
extern Preferences preferences;


#include <Arduino.h>
#include <math.h>

// Define pins for temperature control
const int sensorPin = 39;       // ADC input pin for the thermistor voltage divider (changed from 36 to 39)
const int relayPin = 5;         // Digital pin controlling the pump relay
const int manualResetPin = 14;  // GPIO pin for manual reset input (3V momentary)
float V_supply = 3.3;     // Supply voltage for thermistor circuit (changed from 5.0 to 3.3)

// Set the supply voltage
void setSupplyVoltage(float voltage) {
  if (voltage > 0.0 && voltage <= 5.5) {  // Safety limits
    V_supply = voltage;
    saveFloat("supplyVoltage", V_supply);
  }
}

// Get the current supply voltage
float getSupplyVoltage() {
  return V_supply;
}
// Global variables for sensor calibration and circuit parameters
float thresholdTemperature = 25.0;    // Default threshold (in Celsius)
float tempCalibrationOffset = 0.0;    // Calibration offset (in Celsius) - RENAMED from calibrationOffset
float voltageDividerResistor = 8500.0; // 8.5K ohms (changed from 10000.0)
bool useFahrenheit = true;            // Default display in °F

// Shutdown latch: if true, the pump remains off until a manual reset is performed
bool shutdownLatch = false;  // Changed to start with pump enabled by default

// Track high temperature streak state and start time for web UI
bool highTempStreakActive = false;
time_t highTempEventStart = 0;

// --- High Temperature Event Logging ---
#include "HighTempEvent.h"
#include "HighTempEventStorage.h"
#include "IHighTempEventHandler.h"
#include "HighTempEventLogger.h"

HighTempEvent highTempEvents[HighTempEventStorage::MAX_EVENTS];
int highTempEventIdx = 0;
int highTempEventCount = 0;
HighTempEventStorage highTempEventStorage;
HighTempEventLogger highTempEventLogger;

// OCP: Handler registration
#define MAX_HIGH_TEMP_HANDLERS 4
IHighTempEventHandler* highTempEventHandlers[MAX_HIGH_TEMP_HANDLERS];
int highTempEventHandlerCount = 0;

void registerHighTempEventHandler(IHighTempEventHandler* handler) {
    if (highTempEventHandlerCount < MAX_HIGH_TEMP_HANDLERS) {
        highTempEventHandlers[highTempEventHandlerCount++] = handler;
    }
}

// Save high temp events to NVS (Preferences)
void saveHighTempEvents() {
    highTempEventStorage.save(highTempEvents, highTempEventCount);
}
// Load high temp events from NVS
void loadHighTempEvents() {
    int validCount = highTempEventStorage.load(highTempEvents, HighTempEventStorage::MAX_EVENTS);
    highTempEventCount = validCount;
    highTempEventIdx = validCount; // Use linear indexing until buffer wraps
}
// Log a new high temperature event
void logHighTempEvent(float temp, time_t t) {
    HighTempEvent event;
    event.tempCelsius = temp;
    event.eventTime = t;
    highTempEvents[highTempEventIdx] = event;
    highTempEventIdx = (highTempEventIdx + 1) % HighTempEventStorage::MAX_EVENTS;
    if (highTempEventCount < HighTempEventStorage::MAX_EVENTS) highTempEventCount++;
    // Notify all registered handlers
    for (int i = 0; i < highTempEventHandlerCount; ++i) {
        if (highTempEventHandlers[i]) {
            highTempEventHandlers[i]->onHighTempEvent(event);
        }
    }
}

// Get the number of logged high temp events (max 3)
int getHighTempEventCount() {
    return highTempEventCount;
}
// Get the Nth most recent high temp event (0=most recent)
bool getHighTempEvent(int idx, float* tempOut, time_t* timeOut) {
    if (idx < 0 || idx >= highTempEventCount) return false;
    int realIdx;
    if (highTempEventCount < HighTempEventStorage::MAX_EVENTS) {
        // Linear order after reboot or before buffer is full
        realIdx = highTempEventCount - 1 - idx;
    } else {
        // Circular buffer order after wrap
        realIdx = (highTempEventIdx - 1 - idx + HighTempEventStorage::MAX_EVENTS) % HighTempEventStorage::MAX_EVENTS;
    }
    *tempOut = highTempEvents[realIdx].tempCelsius;
    *timeOut = highTempEvents[realIdx].eventTime;
    return true;
}


// Function to read the thermistor and compute temperature in Celsius
float readTemperature() {
  // Read ADC value
  int adcValue = analogRead(sensorPin);
  
  // Convert to voltage
  float voltage = (adcValue / 4095.0) * V_supply;
  
  // Serial debug output
  Serial.println("---------- Temperature Reading ----------");
  Serial.print("Raw ADC Value: ");
  Serial.println(adcValue);
  Serial.print("V_supply: ");
  Serial.println(V_supply, 3);
  Serial.print("Voltage reading: ");
  Serial.println(voltage, 3);
  
  // Check for invalid voltage reading
  if (voltage <= 0.001 || voltage >= V_supply - 0.001) {
    Serial.println("WARNING: Invalid voltage reading, using default temperature");
    return 25.0; // Return room temperature as fallback
  }
  
  // Calculate thermistor resistance using voltage divider formula
  // This code change can be used to modify for inverted thermistor circuit
  // For a circuit where thermistor is connected to GND and resistor is connected to VCC:
  // R_thermistor = R_divider * (V_measured / (V_supply - V_measured))
  //float R_thermistor = voltageDividerResistor * (voltage / (V_supply - voltage));
 float R_thermistor = voltageDividerResistor * ((V_supply - voltage) / voltage);

  Serial.print("Calculated thermistor resistance: ");
  Serial.print(R_thermistor);
  Serial.println(" ohms");
  
  // Safety check for invalid resistance
  if (R_thermistor <= 0 || R_thermistor > 1000000) {
    Serial.println("WARNING: Invalid resistance calculated, using default temperature");
    return 25.0;
  }
  
  // Use Steinhart-Hart equation to calculate temperature
  // T = 1 / (A + B*ln(R) + C*ln(R)^3)
  // Using simplified B-parameter equation: T = 1 / (T0_INV + (1/B) * ln(R/R0))
  // where T0_INV = 1/T0 (in Kelvin), R0 is resistance at T0 (typically 25°C)
  
  // Common values for 10K NTC thermistor
  const float B_PARAMETER = 3950.0; // B parameter (typical for NTC thermistors)
  const float T0 = 298.15; // 25°C in Kelvin
  const float R0 = 10000.0; // Resistance at 25°C (10K)
  
  float steinhart = log(R_thermistor / R0);
  steinhart = steinhart / B_PARAMETER;
  steinhart = steinhart + (1.0 / T0);
  steinhart = 1.0 / steinhart; // Temperature in Kelvin
  
  // Convert to Celsius
  float tempCelsius = steinhart - 273.15;
  
  // Apply calibration offset
  tempCelsius += tempCalibrationOffset;
  
  Serial.print("Calculated temperature: ");
  Serial.print(tempCelsius);
  Serial.println("°C");
  
  return tempCelsius;
}

// Check for manual reset button press
void checkForManualReset() {
  if (digitalRead(manualResetPin) == HIGH) {
    Serial.println("Manual reset button pressed. Clearing shutdown latch.");
    shutdownLatch = false;
    Serial.println("Shutdown latch cleared. Pump control will now depend on temperature.");
    delay(500); // Debounce delay
  }
}

// Initialize the temperature control system
void initTempControl() {
  // Load persistent high temperature event log
  loadHighTempEvents();

  // Set pin modes
  pinMode(relayPin, OUTPUT);
  pinMode(manualResetPin, INPUT_PULLDOWN);
  pinMode(sensorPin, INPUT);
  
  // First, check the temperature to see if we should start with the pump on or off
  float initialTemp = readTemperature();
  
  // Only start with the pump on if the temperature is safe
  if (initialTemp < thresholdTemperature) {
    shutdownLatch = false;
    digitalWrite(relayPin, HIGH); // Start with pump relay on if temperature is safe
    Serial.println("Initial temperature is safe. Starting with pump ON.");
  } else {
    shutdownLatch = true;
    digitalWrite(relayPin, LOW); // Start with pump relay off if temperature is unsafe
    Serial.println("WARNING: Initial temperature exceeds threshold. Starting with pump OFF.");
  }
  
  Serial.println("Temperature control system initialized");
}

// Update the pump relay based on temperature and shutdown latch
void updateTempControl() {
    const float MIN_VALID_TEMP = 0.0;    // Minimum reasonable pool temp (°C)
    const float MAX_VALID_TEMP = 60.0;   // Maximum reasonable pool temp (°C)
    const int REQUIRED_HIGH_READINGS = 60; // 3 seconds at 20Hz
    static int highTempCounter = 0;

  float tempCelsius = readTemperature();

  // Only consider valid temperature readings
  if (tempCelsius < MIN_VALID_TEMP || tempCelsius > MAX_VALID_TEMP) {
    Serial.println("Ignored out-of-range temperature reading.");
    return; // Ignore this loop
  }

  Serial.print("Current temperature: ");
  Serial.print(tempCelsius);
  Serial.print("°C, Threshold: ");
  Serial.print(thresholdTemperature);
  Serial.print("°C, ShutdownLatch: ");
  Serial.println(shutdownLatch ? "ACTIVE" : "INACTIVE");

  if (shutdownLatch) {
    digitalWrite(relayPin, LOW);
    Serial.println("Shutdown latch is active. Pump is OFF.");
    highTempCounter = 0;
    highTempStreakActive = false;
    highTempEventStart = 0;
  } else if (tempCelsius >= thresholdTemperature) {
    // Log a high temp event every time a reading exceeds the threshold
    logHighTempEvent(tempCelsius, time(nullptr));
    if (highTempCounter == 0) {
      highTempEventStart = time(nullptr); // Record the start time of the event
      highTempStreakActive = true;
    }
    highTempCounter++;
    if (highTempCounter >= REQUIRED_HIGH_READINGS) {
      Serial.println("High temperature detected for 3 seconds! Activating shutdown latch.");
      shutdownLatch = true;
      digitalWrite(relayPin, LOW);
      highTempStreakActive = false;
      highTempEventStart = 0;
    }
  } else {
    highTempCounter = 0; // Reset counter if temp drops below threshold
    highTempStreakActive = false;
    highTempEventStart = 0;
    digitalWrite(relayPin, HIGH);
    Serial.println("Temperature is safe. Pump is ON.");
  }

  Serial.print("Relay pin state: ");
  Serial.println(digitalRead(relayPin) ? "HIGH (Pump ON)" : "LOW (Pump OFF)");

  checkForManualReset();
}

// Getter for high temp streak active
bool isHighTempStreakActive() {
    return highTempStreakActive;
}

// Getter for high temp event start time
// Returns 0 if not active
unsigned long getHighTempEventStartTime() {
    return (unsigned long)highTempEventStart;
}


// Get current temperature in Celsius
float getTemperatureCelsius() {
  return readTemperature();
}

// Get current temperature in display units (Celsius or Fahrenheit)
float getDisplayTemperature() {
  float tempCelsius = readTemperature();
  if (useFahrenheit) {
    return tempCelsius * 9.0 / 5.0 + 32.0;
  }
  return tempCelsius;
}

// Get the threshold temperature in display units
float getDisplayThreshold() {
  if (useFahrenheit) {
    return thresholdTemperature * 9.0 / 5.0 + 32.0;
  }
  return thresholdTemperature;
}

// Set the threshold temperature (in display units)
void setThresholdTemperature(float threshold) {
  if (useFahrenheit) {
    thresholdTemperature = (threshold - 32.0) * 5.0 / 9.0;
  } else {
    thresholdTemperature = threshold;
  }
  saveFloat("tempThreshold", thresholdTemperature);
}

// Set the calibration offset (in display units)
void setCalibrationOffset(float offset) {
  if (useFahrenheit) {
    tempCalibrationOffset = offset * 5.0 / 9.0;
  } else {
    tempCalibrationOffset = offset;
  }
  saveFloat("tempCalOffset", tempCalibrationOffset);
}

// Set the voltage divider resistor value
void setDividerResistor(float resistance) {
  voltageDividerResistor = resistance;
  saveFloat("divResistor", voltageDividerResistor);
}

// Toggle between Celsius and Fahrenheit
void toggleTemperatureUnit(bool useF) {
  useFahrenheit = useF;
  saveBool("useFahrenheit", useFahrenheit);
}

// Get whether temperature is displayed in Fahrenheit (true) or Celsius (false)
bool isFahrenheitUnit() {
  return useFahrenheit;
}

// Check if the pump is currently shut off
bool isPumpShutoff() {
  return (digitalRead(relayPin) == LOW);
}

// Check if the shutdown latch is active
bool isShutdownLatched() {
  return shutdownLatch;
}

// Reset the shutdown latch
void resetShutdownLatch() {
  shutdownLatch = false;
}