#ifndef HIGHTEMPEVENTSTORAGE_H
#define HIGHTEMPEVENTSTORAGE_H

#include "HighTempEvent.h"
#include "PreferencesManager.h"
#include "IHighTempEventHandler.h"

class HighTempEventStorage : public IHighTempEventHandler {
public:
    static const int MAX_EVENTS = 3;
    void save(const HighTempEvent* events, int count);
    int load(HighTempEvent* events, int maxCount);
    // OCP: Handler interface
    void onHighTempEvent(const HighTempEvent& event) override;
};

#endif // HIGHTEMPEVENTSTORAGE_H
