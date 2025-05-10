#include "PreferencesManager.h"
#include <Preferences.h>

Preferences preferences;

// External variables that need to be loaded
extern int numReadings;
extern float calibrationOffset;
extern float calibrationSlope;
extern float targetPH;
extern float desiredCO2PerPulse_lbs;
extern unsigned long manualPulseLength;
extern unsigned long pulseInterval;
extern int desiredPulseCount;
extern int scheduledHour1;
extern int scheduledMinute1;
extern int scheduledHour2;
extern int scheduledMinute2;
extern int currentDay;
extern float dailyAvg[10];
extern int pulsesRemaining;  // Added to load pulse count

// ── Time‑zone storage ───────────────────────
String tzName;  // will hold e.g. "EST5EDT,M3.2.0/2/2,M11.1.0/2"


void loadSettings() {
  preferences.begin("settings", true); // read-only mode
  
  // Load temperature control settings
extern float V_supply;  // From TempControl.cpp
V_supply = preferences.getFloat("supplyVoltage", 5.0);  // Default 5.0V
extern float thresholdTemperature;
extern float tempCalibrationOffset;
extern float voltageDividerResistor;
extern bool useFahrenheit;

thresholdTemperature    = preferences.getFloat("tempThreshold", 25.0);
tempCalibrationOffset   = preferences.getFloat("tempCalOffset", 0.0);
voltageDividerResistor  = preferences.getFloat("divResistor", 8500.0);
useFahrenheit           = preferences.getBool("useFahrenheit", true);

  // Load sensor settings
  numReadings = preferences.getInt("numReadings", 30);
  
  // Load calibration settings
  calibrationOffset = preferences.getFloat("calOffset", 0.0);
  calibrationSlope = preferences.getFloat("calSlope", 1.0);
  
  // Load target pH
  targetPH = preferences.getFloat("targetPH", 7.4);
  
  // Load CO2 injection settings
  desiredCO2PerPulse_lbs = preferences.getFloat("co2PerPulse", 1.2);
  manualPulseLength = preferences.getULong("pulseLength", 60000);
  pulseInterval = preferences.getULong("pulseInterval", 60UL * 60000UL);
  desiredPulseCount = preferences.getInt("pulseCount", 5);
  pulsesRemaining = preferences.getInt("pulsesRemaining", 0);  // Added to load pulse count
  
  // Load schedule settings
  scheduledHour1 = preferences.getInt("schHour1", 9);
  scheduledMinute1 = preferences.getInt("schMin1", 0);
  scheduledHour2 = preferences.getInt("schHour2", 13);
  scheduledMinute2 = preferences.getInt("schMin2", 0);
  
  // Load trend data
  currentDay = preferences.getInt("curDay", -1);
  for (int i = 0; i < 10; i++) {
    String key = "avg" + String(i);
    dailyAvg[i] = preferences.getFloat(key.c_str(), 0.0);
  }
  // Load TZ (default = US Eastern w/ DST)
tzName = preferences.getString(
  "tz",
  "EST5EDT,M3.2.0/2/2,M11.1.0/2"
);

  preferences.end();
}

void saveFloat(const char* key, float value) {
  preferences.begin("settings", false);
  preferences.putFloat(key, value);
  preferences.end();
}

float getFloat(const char* key, float defaultValue) {
  preferences.begin("settings", true);
  float val = preferences.getFloat(key, defaultValue);
  preferences.end();
  return val;
}

unsigned long getULong(const char* key, unsigned long defaultValue) {
  preferences.begin("settings", true);
  unsigned long val = preferences.getULong(key, defaultValue);
  preferences.end();
  return val;
}

void saveInt(const char* key, int value) {
  preferences.begin("settings", false);
  preferences.putInt(key, value);
  preferences.end();
}

void saveULong(const char* key, unsigned long value) {
  preferences.begin("settings", false);
  preferences.putULong(key, value);
  preferences.end();
}
// Save a string into Preferences
void saveString(const char* key, const char* value) {
  preferences.begin("settings", false);
  preferences.putString(key, value);
  preferences.end();
}


// Add this function to PreferencesManager.cpp

void saveBool(const char* key, bool value) {
  preferences.begin("settings", false);
  preferences.putBool(key, value);
  preferences.end();
}