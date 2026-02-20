#ifndef DEBUG_LOGGER_H
#define DEBUG_LOGGER_H

#include <Arduino.h>

// Set to 0 to disable all logging and save flash/RAM
#define DEBUG_ENABLED 1

#if DEBUG_ENABLED
    #define LOG_INIT(baud)      Serial.begin(baud)
    #define LOG_ERROR(msg)      Serial.println(F("[ERROR] " msg))
    #define LOG_WARN(msg)       Serial.println(F("[WARN]  " msg))
    #define LOG_INFO(msg)       Serial.println(F("[INFO]  " msg))
    #define LOG_DEBUG(msg)      Serial.println(F("[DEBUG] " msg))
    #define LOG_VAR(label, val) Serial.print(F("[VAR]   " label ": ")); Serial.println(val)
#else
    #define LOG_INIT(baud)
    #define LOG_ERROR(msg)
    #define LOG_WARN(msg)
    #define LOG_INFO(msg)
    #define LOG_DEBUG(msg)
    #define LOG_VAR(label, val)
#endif

#endif
