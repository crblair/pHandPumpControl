#ifndef INJECTIONCONTROL_H
#define INJECTIONCONTROL_H

// Initialize the injection control system
void initInjectionControl();

// Update injection control state (call this in main loop)
void updateInjectionControl();

// Check if scheduled injection times match current time
void checkScheduledInjections();

// Start a new injection series with the specified number of pulses
void startInjectionSeries(int pulseCount);

// Decrement remaining pulse count (called after each pulse completes)
void decrementPulseCount();

// Get current status
bool isInjectionActive();
int getPulsesRemaining();
unsigned long getNextPulseTime();

#endif // INJECTIONCONTROL_H