#include "InjectionControl.h"
#include "PreferencesManager.h"
#include <Arduino.h>
#include <time.h>

// Define the pin for controlling the CO2 valve
const int valvePin = 2;  // Change this to match your actual pin number

// External variables from WebHandler.cpp
extern int pulsesRemaining;
extern unsigned long manualPulseLength;
extern unsigned long pulseInterval;
extern int desiredPulseCount;
extern int scheduledHour1;
extern int scheduledMinute1;
extern int scheduledHour2;
extern int scheduledMinute2;

// Internal state variables
bool injectionActive = false;
bool valveOpen = false;
unsigned long pulseStartTime = 0;
unsigned long nextPulseTime = 0;
int lastScheduleCheckMinute = -1; // Store the last minute we checked for scheduled runs

// Initialize the injection control system
void initInjectionControl() {
  // Set up the valve control pin
  pinMode(valvePin, OUTPUT);
  digitalWrite(valvePin, LOW);  // Ensure valve is closed on startup
  
  injectionActive = (pulsesRemaining > 0);
  valveOpen = false;
  
  // If we were in the middle of an injection series when reset, continue
  if (injectionActive) {
    nextPulseTime = millis() + 5000;  // Start the first pulse after 5 seconds
  }
}

// Check if a scheduled injection should start
void checkScheduledInjections() {
  time_t now = time(nullptr);
  if (now < 8 * 3600) {
    // Time not set yet, skip check
    return;
  }
  
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  // Only check once per minute to avoid multiple triggers
  if (timeinfo.tm_min == lastScheduleCheckMinute) {
    return;
  }
  
  lastScheduleCheckMinute = timeinfo.tm_min;
  
  // Check if current time matches either of the scheduled times
  if ((timeinfo.tm_hour == scheduledHour1 && timeinfo.tm_min == scheduledMinute1) ||
      (timeinfo.tm_hour == scheduledHour2 && timeinfo.tm_min == scheduledMinute2)) {
    
    // Don't start a new series if one is already in progress
    if (!injectionActive || pulsesRemaining == 0) {
      Serial.println("Starting scheduled injection series");
      startInjectionSeries(desiredPulseCount);
    }
  }
}

// Update injection control state (call this in main loop)
void updateInjectionControl() {
  // Check if it's time for a scheduled injection
  checkScheduledInjections();
  
  unsigned long currentTime = millis();
  
  // If injection is active and it's time for a new pulse
  if (injectionActive && !valveOpen && currentTime >= nextPulseTime && pulsesRemaining > 0) {
    // Open the valve
    digitalWrite(valvePin, HIGH);
    valveOpen = true;
    pulseStartTime = currentTime;
    
    Serial.println("Valve opened - pulse started");
  }
  
  // If the valve is open and pulse duration has elapsed
  if (valveOpen && currentTime - pulseStartTime >= manualPulseLength) {
    // Close the valve
    digitalWrite(valvePin, LOW);
    valveOpen = false;
    
    // Decrement pulse count
    decrementPulseCount();
    
    Serial.print("Pulse completed. Remaining pulses: ");
    Serial.println(pulsesRemaining);
    
    // Schedule next pulse if any remain
    if (pulsesRemaining > 0) {
      nextPulseTime = currentTime + pulseInterval;
    } else {
      injectionActive = false;
      Serial.println("Injection series completed");
    }
  }
}

// Start a new injection series with the specified number of pulses
void startInjectionSeries(int pulseCount) {
  // Stop any ongoing injection first
  digitalWrite(valvePin, LOW);
  valveOpen = false;
  
  // Set the pulse count
  pulsesRemaining = pulseCount;
  saveInt("pulsesRemaining", pulsesRemaining);
  
  // Set the system to active and schedule the first pulse
  injectionActive = true;
  nextPulseTime = millis() + 2000;  // Start first pulse in 2 seconds
  
  Serial.print("Starting new injection series with ");
  Serial.print(pulseCount);
  Serial.println(" pulses");
}

// Decrement remaining pulse count (called after each pulse completes)
void decrementPulseCount() {
  if (pulsesRemaining > 0) {
    pulsesRemaining--;
    saveInt("pulsesRemaining", pulsesRemaining);
  }
}

// Get current status
bool isInjectionActive() {
  return injectionActive;
}

int getPulsesRemaining() {
  return pulsesRemaining;
}

unsigned long getNextPulseTime() {
  return nextPulseTime;
}