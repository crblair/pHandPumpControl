#include "Sensors.h"
#include <Arduino.h>
#include <time.h>
#include <math.h>
#include "PreferencesManager.h"

// Global variables for running average.
int numReadings = 30;         // Default value; loaded via PreferencesManager.
float *pHReadings = nullptr;
int readingIndex = 0;
int readingCount = 0;

// Global variables for daily trend.
unsigned long daySum = 0;   // Sum of readings scaled by 1000.
int dayCount = 0;
int currentDay = -1;        // tm_yday value.
float dailyAvg[10] = {0};   // Past 10 days average.

// pH sensor pin.
const int pHSensorPin = 34;

// Allocate (or reallocate) the running-average buffer.
void allocatePHBuffer() {
  if (pHReadings != nullptr) {
    delete[] pHReadings;
  }
  pHReadings = new float[numReadings];
  readingIndex = 0;
  readingCount = 0;
}

void initSensors() {
  // numReadings should be loaded already via loadSettings().
  allocatePHBuffer();
}

// Returns uncalibrated raw reading from the pH sensor.
float getRawValue() {
  int sensorValue = analogRead(pHSensorPin);
  float voltage = sensorValue * (3.3 / 4095.0);
  return 7.0 + (1.65 - voltage);
}

// External calibration variables defined in WebHandler.cpp.
extern float calibrationOffset;
extern float calibrationSlope;

// Read pH with minimal debug output
float readPH() {
  float raw = getRawValue();
  float calibrated = 7.0 + ((raw - 7.0) * calibrationSlope) + calibrationOffset;
  
  // Keep minimal debug output
  Serial.print("Raw pH: ");
  Serial.print(raw);
  Serial.print(", Calibrated pH: ");
  Serial.println(calibrated);
  
  return calibrated;
}

// Update running average using a circular buffer with minimal debugging
void updatePHBuffer(float newReading) {
  // Remove excessive debug output to improve performance
  pHReadings[readingIndex] = newReading;
  readingIndex = (readingIndex + 1) % numReadings;
  if (readingCount < numReadings) readingCount++;
}

// Get running average pH with minimal debugging
float getRunningPH() {
  if (readingCount == 0) return 0; // Safety check for empty buffer
  
  float sum = 0;
  for (int i = 0; i < readingCount; i++) {
    sum += pHReadings[i];
  }
  
  float average = sum / readingCount;
  
  // Keep minimal debug output
  Serial.print("pH Running Average: ");
  Serial.println(average);
  
  return average;
}

float computeMassFlowRate() {
  float Cd = 0.8;
  float tubeDiameter = 0.00635; // 1/4" in meters
  float tubeRadius = tubeDiameter / 2.0;
  float area = PI * tubeRadius * tubeRadius;
  float P0 = 300000.0;  // Pa
  float gamma = 1.3;
  float R_specific = 188.9; // J/(kg·K)
  float T = 298.0;        // Kelvin
  return Cd * area * P0 * sqrt((gamma / (R_specific * T)) *
           pow((2.0 / (gamma + 1.0)), ((gamma + 1.0) / (gamma - 1.0))));
}

float computeChokeDifferentialPressure() {
  float gamma = 1.3;
  float P0 = 300000.0;  // Pa
  float criticalRatio = pow(2.0 / (gamma + 1.0), gamma / (gamma - 1.0));
  return P0 * (1 - criticalRatio);
}

// Update daily trend: call this periodically.
void updateDailyTrend() {
  time_t t = time(nullptr);
  struct tm localT;
  localtime_r(&t, &localT);
  int today = localT.tm_yday;
  
  if (currentDay == -1) {
    currentDay = today;
    daySum = 0;
    dayCount = 0;
  } else if (today != currentDay) {
    if (dayCount > 0) {
      // Correctly calculate average from the accumulated sum
      float avg = (float)daySum / (dayCount * 1000.0);
      
      // Debug output for tracking daily averages
      Serial.print("Day changed. New average for day ");
      Serial.print(currentDay);
      Serial.print(": ");
      Serial.println(avg);
      
      // Shift dailyAvg array left.
      for (int i = 0; i < 9; i++) {
        dailyAvg[i] = dailyAvg[i + 1];
        String key = "avg" + String(i);
        saveFloat(key.c_str(), dailyAvg[i]);
      }
      
      dailyAvg[9] = avg;
      saveFloat("avg9", avg);
    }
    
    // Reset for the new day
    currentDay = today;
    saveInt("curDay", currentDay);
    daySum = 0;
    dayCount = 0;
    
    // Debug output for tracking day changes
    Serial.print("Day changed to: ");
    Serial.println(currentDay);
  }
}

// Streamlined updateSensors function for better performance
void updateSensors() {
  float current = readPH();
  updatePHBuffer(current);
  
  // Scale by 1000 to preserve decimals in the unsigned long daySum
  daySum += (unsigned long)(current * 1000);
  dayCount++;
  
  updateDailyTrend();
}