#ifndef EEPROM_MANAGER_H
#define EEPROM_MANAGER_H

#include <EEPROM.h>
#include <Arduino.h>
#include "DebugLogger.h"

#define EEPROM_MAGIC 0xA1B2C3DA
#define MAX_PROFILES 5
#define MAX_STEPS 8

struct __attribute__((packed)) PIDConstants {
    double kp;
    double ki;
    double kd;
    double iBand;
    double dFilter;
};

enum StepType : uint8_t { 
    STEP_RAMP_TIME = 0, 
    STEP_SOAK_TIME = 1,
    STEP_RAMP_RATE = 2,
    STEP_HOLD_STABLE = 3,
    STEP_WAIT_INPUT = 4
};

struct __attribute__((packed)) ProfileStep {
    StepType type;
    float targetTemp;
    unsigned long durationSec;
};

struct __attribute__((packed)) Profile {
    char name[16];
    uint8_t stepCount;
    ProfileStep steps[MAX_STEPS];
};

struct __attribute__((packed)) SystemData {
    uint32_t magic;
    PIDConstants pid;
    Profile profiles[MAX_PROFILES];
    char wifiSSID[32];
    char wifiPass[64];
    char hostname[32];
    uint8_t sensorType; // 0=DS18B20, 1=MAX31856
    uint8_t tcType;     // 0=B, 1=E, 2=J, 3=K, 4=N, 5=R, 6=S, 7=T
    float safetyLimit;
    uint32_t crc;
};

class EEPROMManager {
public:
    SystemData data;

    void begin() {
        LOG_VAR("EEPROM Struct Size", sizeof(SystemData));
        LOG_VAR("EEPROM Total Size", EEPROM.length());
        load();
    }

    void load() {
        EEPROM.get(0, data);
        uint32_t calcCRC = calculateCRC();
        
        if (data.magic != EEPROM_MAGIC || data.crc != calcCRC) {
            LOG_WARN("EEPROM Corrupt or Uninitialized. Loading Defaults.");
            LOG_VAR("Read Magic", data.magic);
            LOG_VAR("Exp Magic", (uint32_t)EEPROM_MAGIC);
            LOG_VAR("Read CRC", data.crc);
            LOG_VAR("Calc CRC", calcCRC);
            loadDefaults();
            save();
        } else {
            LOG_INFO("EEPROM Loaded Successfully.");
        }
    }

    void save() {
        data.crc = calculateCRC();
        EEPROM.put(0, data);
        
        // Verify write
        uint32_t savedMagic;
        EEPROM.get(0, savedMagic);
        if (savedMagic != data.magic) {
            LOG_ERROR("EEPROM Write Verification Failed!");
        } else {
            LOG_INFO("EEPROM Saved.");
        }
    }

    void loadDefaults() {
        memset(&data, 0, sizeof(SystemData)); // Zero out to ensure deterministic padding/CRC
        data.magic = EEPROM_MAGIC;
        data.pid = {20.0, 0.5, 1.0, 5.0, 1.0}; // Conservative defaults, 1.0 = no filter
        
        // Default Profile 1: Reflow
        strcpy(data.profiles[0].name, "Reflow Demo");
        data.profiles[0].stepCount = 3;
        data.profiles[0].steps[0] = {STEP_RAMP_TIME, 150.0, 60};
        data.profiles[0].steps[1] = {STEP_SOAK_TIME, 150.0, 90};
        data.profiles[0].steps[2] = {STEP_RAMP_TIME, 220.0, 60};

        // Clear others
        for(int i=1; i<MAX_PROFILES; i++) {
            sprintf(data.profiles[i].name, "Empty %d", i);
            data.profiles[i].stepCount = 0;
        }
        data.wifiSSID[0] = 0;
        data.wifiPass[0] = 0;
        strcpy(data.hostname, "r4-controller");
        data.sensorType = 0; // Default DS18B20
        data.tcType = 3;     // Default Type K
        data.safetyLimit = 250.0; // Default Safety Limit
    }

private:
    uint32_t calculateCRC() {
        const uint8_t* p = (const uint8_t*)&data;
        uint32_t crc = 0;
        // Calculate CRC over data excluding the CRC field itself
        for (size_t i = 0; i < sizeof(SystemData) - sizeof(uint32_t); i++) {
            crc += p[i];
            crc += (crc << 10);
            crc ^= (crc >> 6);
        }
        crc += (crc << 3);
        crc ^= (crc >> 11);
        crc += (crc << 15);
        return crc;
    }
};

#endif
