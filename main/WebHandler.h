#ifndef WEBHANDLER_H
#define WEBHANDLER_H

#include <WebServer.h>

// Setup web server routes
void setupWebHandlers(WebServer &server);

// Main page handlers
void handleRoot();
void handleTemperatureControl();

// Firmware update related handlers
void handleUpdate();
void handleDoUpdate();
void handleUpdateDone();

// pH/CO2 System form handlers
void handleTrend();
void handleSetSchedule1();
void handleSetSchedule2();
void handleSetNumReadings();
void handleSetCalOffset();
void handleSetCalSlope();
void handleToggleInjection();
void handleSetCO2();
void handleSetPH();
void handleSetPulseInterval();
void handleSetPulseCount();
void handleSetPulseLength();
// Time‑zone selector
void handleSetTimezone();


// Temperature Control form handlers
void handleSetThreshold();
void handleSetTempCalOffset();
void handleSetDividerResistor();
void handleSetTempUnit();
void handleResetPumpShutdown();
void handleSetVoltage();  // New handler for setting supply voltage

#endif  // WEBHANDLER_H