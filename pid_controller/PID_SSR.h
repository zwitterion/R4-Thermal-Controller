#ifndef PID_SSR_H
#define PID_SSR_H

#include <Arduino.h>
#include "DebugLogger.h"

class PID_SSR {
public:
    double kp, ki, kd;
    double input, output, setpoint;
    
    // SSR Smoothing
    unsigned long windowSize = 2000; // 2 seconds
    unsigned long minSwitchTime = 200; // 200ms min ON/OFF
    double integralSeparationBand = 5.0;
    double dFilter = 1.0; // 1.0 = No Filter, 0.0 = Infinite Filter
    
    // Autotune
    bool isTuning = false;
    
    PID_SSR(int pin) : ssrPin(pin) {
        pinMode(ssrPin, OUTPUT);
    }

    void begin(double p, double i, double d) {
        kp = p; ki = i; kd = d;
        windowStartTime = millis();
    }

    // Call this as fast as possible in loop()
    void update(double currentTemp) {
        input = currentTemp;
        unsigned long now = millis();

        if (isTuning) {
            runAutotune(now);
        } else {
            computePID(now);
        }

        processSSRSmoothing(now);
    }

    void startAutotune(double target) {
        setpoint = target;
        isTuning = true;
        tuneCycleCount = 0;
        tuneState = 0; // 0: Heat, 1: Cool
        tuneMax = -1000;
        tuneMin = 1000;
        output = 100.0; // Start heating
        tuneStartTime = millis();
        integral = 0; // Reset integral to prevent windup from previous state
        LOG_INFO("Autotune Started");
    }

    void cancelAutotune() {
        isTuning = false;
        output = 0;
        LOG_INFO("Autotune Cancelled");
    }

private:
    int ssrPin;
    double integral = 0, lastError = 0, lastDerivative = 0;
    unsigned long lastPIDTime = 0;
    
    // Smoothing Vars
    unsigned long windowStartTime;
    bool ssrState = false;
    unsigned long lastSwitchTime = 0;

    // Autotune Vars
    int tuneCycleCount = 0;
    int tuneState = 0;
    double tuneMax, tuneMin;
    unsigned long tuneStartTime;
    unsigned long t1, t2; // Peak timestamps

    void computePID(unsigned long now) {
        if (now - lastPIDTime < 200) return; // 200ms sample time
        double dt = (double)(now - lastPIDTime) / 1000.0;
        lastPIDTime = now;

        const double error = setpoint - input;
        updateIntegral(error, dt);
        
        double rawDerivative = (error - lastError) / dt;
        double derivative = (dFilter * rawDerivative) + ((1.0 - dFilter) * lastDerivative);
        lastDerivative = derivative;
        lastError = error;

        double out = (kp * error) + (ki * integral) + (kd * derivative);
        output = constrain(out, 0.0, 100.0);
    }

    void updateIntegral(double error, double dt) {
        if (abs(error) < integralSeparationBand) {
            // Accumulate integral only when close to the setpoint
            integral += error * dt;
            // Anti-windup: clamp the integral to prevent excessive buildup
            integral = constrain(integral, -100.0, 100.0); 
        } else {
            // Reset integral when far from the setpoint
            integral = 0;
        }
    }

    void runAutotune(unsigned long now) {
        // Relay Method
        double hysteresis = 0.5;
        
        if (tuneState == 0) { // Heating
            output = 100.0;
            if (input > setpoint + hysteresis) {
                tuneState = 1;
                tuneMax = input;
                t1 = now;
            }
        } else { // Cooling
            output = 0.0;
            if (input < setpoint - hysteresis) {
                tuneState = 0;
                tuneMin = input;
                t2 = now;
                tuneCycleCount++;
                LOG_VAR("Tune Cycle", tuneCycleCount);
            }
        }

        // Track peaks
        if (input > tuneMax) tuneMax = input;
        if (input < tuneMin) tuneMin = input;

        if (tuneCycleCount >= 3) {
            finishAutotune();
        }
    }

    void finishAutotune() {
        isTuning = false;
        // Ziegler-Nichols Calculation
        double Ku = (4.0 * 100.0) / (3.14159 * (tuneMax - tuneMin));
        double Tu = (double)(t2 - t1) * 2.0 / 1000.0; // Period in seconds

        // PID Classic
        kp = 0.6 * Ku;
        ki = 1.2 * Ku / Tu;
        kd = 0.075 * Ku * Tu;

        LOG_INFO("Autotune Complete.");
        LOG_VAR("New Kp", kp);
        LOG_VAR("New Ki", ki);
        LOG_VAR("New Kd", kd);
        output = 0;
    }

    void processSSRSmoothing(unsigned long now) {
        // Time Proportional Window Logic
        if (now - windowStartTime > windowSize) {
            windowStartTime += windowSize;
        }
        
        unsigned long windowElapsed = now - windowStartTime;
        double onTime = (output / 100.0) * windowSize;
        
        bool desiredState = (windowElapsed < onTime);

        // Minimum ON/OFF Duration Enforcement
        if (desiredState != ssrState) {
            if (now - lastSwitchTime >= minSwitchTime) {
                ssrState = desiredState;
                digitalWrite(ssrPin, ssrState ? HIGH : LOW);
                lastSwitchTime = now;
            }
            // Else: wait until min time passes, effectively ignoring micro-bursts
        }
    }
};

#endif
