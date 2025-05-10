#ifndef TEMPCONTROL_H
#define TEMPCONTROL_H

#include <time.h>

// Set the supply voltage
void setSupplyVoltage(float voltage);

// Get the current supply voltage
float getSupplyVoltage();

// Initialize the temperature control system
void initTempControl();

// Update temperature control state (call this in main loop)
void updateTempControl();

// Check for manual reset of the shutdown latch
void checkForManualReset();

// Get current temperature in Celsius
float getTemperatureCelsius();

// High temp streak status and event time
bool isHighTempStreakActive();
unsigned long getHighTempEventStartTime();

// High temperature event log (last 3 events)
int getHighTempEventCount();
bool getHighTempEvent(int idx, float* tempOut, time_t* timeOut);
void saveHighTempEvents();

// Get current temperature in display units (Celsius or Fahrenheit)
float getDisplayTemperature();

// Get the threshold temperature in display units
float getDisplayThreshold();

// Set the threshold temperature (in display units)
void setThresholdTemperature(float threshold);

// Set the calibration offset (in display units)
void setCalibrationOffset(float offset);

// Set the voltage divider resistor value
void setDividerResistor(float resistance);

// Toggle between Celsius and Fahrenheit
void toggleTemperatureUnit(bool useFahrenheit);

// Get whether temperature is displayed in Fahrenheit (true) or Celsius (false)
bool isFahrenheitUnit();

// Check if the pump is currently shut off
bool isPumpShutoff();

// Check if the shutdown latch is active
bool isShutdownLatched();

// Reset the shutdown latch
void resetShutdownLatch();

#endif // TEMPCONTROL_H