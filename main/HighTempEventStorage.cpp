#include "HighTempEvent.h"
#include "HighTempEventStorage.h"
#include "PreferencesManager.h"

void HighTempEventStorage::save(const HighTempEvent* events, int count) {
    for (int i = 0; i < MAX_EVENTS; ++i) {
        char keyTemp[24];
        char keyTime[28];
        snprintf(keyTemp, sizeof(keyTemp), "highTempEvent%d", i);
        snprintf(keyTime, sizeof(keyTime), "highTempEventTime%d", i);
        float temp = (i < count) ? events[i].tempCelsius : 0.0f;
        time_t t = (i < count) ? events[i].eventTime : 0UL;
        saveFloat(keyTemp, temp);
        saveULong(keyTime, (unsigned long)t);
    }
    saveInt("highTempEventCount", count);
}

void HighTempEventStorage::onHighTempEvent(const HighTempEvent& event) {
    // TODO: In a full system, this would append the event to a buffer and call save().
    // For demonstration, just save this single event as the only entry.
    save(&event, 1);
}
int HighTempEventStorage::load(HighTempEvent* events, int maxCount) {
    int validCount = 0;
    for (int i = 0; i < MAX_EVENTS && i < maxCount; ++i) {
        char keyTemp[24];
        char keyTime[28];
        snprintf(keyTemp, sizeof(keyTemp), "highTempEvent%d", i);
        snprintf(keyTime, sizeof(keyTime), "highTempEventTime%d", i);
        float temp = getFloat(keyTemp, 0.0f);
        time_t t = (time_t)getULong(keyTime, 0UL);
        if (t > 0 && temp != 0.0f) {
            events[validCount].tempCelsius = temp;
            events[validCount].eventTime = t;
            ++validCount;
        }
    }
    return validCount;
}
