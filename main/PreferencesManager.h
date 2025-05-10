#include <Arduino.h>    // <=== Add this so “String” is defined
#ifndef PREFERENCESMANAGER_H
#define PREFERENCESMANAGER_H



// Functions for persistent settings
void loadSettings();
void saveFloat(const char* key, float value);
void saveInt(const char* key, int value);
void saveULong(const char* key, unsigned long value);
void saveBool(const char* key, bool value);  // Added for boolean settings
float getFloat(const char* key, float defaultValue);
unsigned long getULong(const char* key, unsigned long defaultValue);

// ── Time‑zone storage ───────────────────────
extern String tzName;                   // POSIX TZ string (e.g. "EST5EDT…")
void saveString(const char* key, const char* value);


#endif  // PREFERENCESMANAGER_H