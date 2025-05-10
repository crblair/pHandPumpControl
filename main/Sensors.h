#ifndef SENSORS_H
#define SENSORS_H

// Initialize sensor system (allocate buffers, etc.)
void initSensors();

// Allocate/reallocate the pH reading buffer
void allocatePHBuffer();

// Update sensor readings; call this periodically.
void updateSensors();

// pH reading functions.
float getRawValue();
float readPH();
void updatePHBuffer(float newReading);
float getRunningPH();

// Daily trend function.
void updateDailyTrend();

// Flow calculation functions.
float computeChokeDifferentialPressure();
float computeMassFlowRate();

#endif  // SENSORS_H