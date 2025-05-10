#ifndef HIGHTEMPEVENT_H
#define HIGHTEMPEVENT_H

#include <time.h>

struct HighTempEvent {
    float tempCelsius;
    time_t eventTime;
};

#endif // HIGHTEMPEVENT_H
