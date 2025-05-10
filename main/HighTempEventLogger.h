#ifndef HIGHTEMPEVENTLOGGER_H
#define HIGHTEMPEVENTLOGGER_H

#include "IHighTempEventHandler.h"
#include <Arduino.h>

class HighTempEventLogger : public IHighTempEventHandler {
public:
    void onHighTempEvent(const HighTempEvent& event) override {
        Serial.print("[LOG] High Temp Event: ");
        Serial.print(event.tempCelsius);
        Serial.print(" C at ");
        Serial.println((unsigned long)event.eventTime);
    }
};

#endif // HIGHTEMPEVENTLOGGER_H
