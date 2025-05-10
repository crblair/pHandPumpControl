#ifndef IHIGHTEMPEVENTHANDLER_H
#define IHIGHTEMPEVENTHANDLER_H

#include "HighTempEvent.h"

class IHighTempEventHandler {
public:
    virtual ~IHighTempEventHandler() {}
    virtual void onHighTempEvent(const HighTempEvent& event) = 0;
};

#endif // IHIGHTEMPEVENTHANDLER_H
