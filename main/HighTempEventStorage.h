#ifndef HIGHTEMPEVENTSTORAGE_H
#define HIGHTEMPEVENTSTORAGE_H

#include <time.h>
#include "PreferencesManager.h"

struct HighTempEvent {
    float tempCelsius;
    time_t eventTime;
};

class HighTempEventStorage {
public:
    static const int MAX_EVENTS = 3;
    void save(const HighTempEvent* events, int count);
    int load(HighTempEvent* events, int maxCount);
};

#endif // HIGHTEMPEVENTSTORAGE_H
