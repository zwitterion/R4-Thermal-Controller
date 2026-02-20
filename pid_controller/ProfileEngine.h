#ifndef PROFILE_ENGINE_H
#define PROFILE_ENGINE_H

#include "EEPROM_Manager.h"

class ProfileEngine {
public:
    bool isRunning = false;
    bool isPaused = false;
    int currentStepIndex = 0;
    unsigned long stepStartTime = 0;
    unsigned long pauseStartTime = 0;
    float startTemp = 0;
    
    // Stability check vars
    float stabilityRefTemp = 0;
    unsigned long stabilityRefTime = 0;
    
    Profile* activeProfile = nullptr;

    void start(Profile* p, float currentTemp) {
        activeProfile = p;
        currentStepIndex = 0;
        isRunning = true;
        isPaused = false;
        startTemp = currentTemp;
        stepStartTime = millis();
        initStep(currentTemp);
    }

    void stop() {
        isRunning = false;
        isPaused = false;
        activeProfile = nullptr;
    }

    void pause() {
        if (isRunning && !isPaused) {
            isPaused = true;
            pauseStartTime = millis();
        }
    }

    void resume() {
        if (isRunning && isPaused) {
            isPaused = false;
            // Adjust start time to account for pause duration
            stepStartTime += (millis() - pauseStartTime);
        }
    }

    // Returns the calculated Setpoint based on time
    float update(float currentTemp) {
        if (!isRunning || !activeProfile) return 0.0;
        if (isPaused) return activeProfile->steps[currentStepIndex].targetTemp; // Hold current target

        unsigned long now = millis();
        unsigned long elapsedSec = (now - stepStartTime) / 1000;
        ProfileStep* step = &activeProfile->steps[currentStepIndex];

        bool stepComplete = false;

        // --- Step Completion Logic ---
        if (step->type == STEP_RAMP_TIME || step->type == STEP_SOAK_TIME) {
            if (elapsedSec >= step->durationSec) stepComplete = true;
        }
        else if (step->type == STEP_RAMP_RATE) {
            // durationSec holds Rate * 100 (deg/min)
            float ratePerSec = (float)step->durationSec / 100.0 / 60.0;
            if (ratePerSec <= 0.0001) stepComplete = true; // Safety
            else {
                float target = step->targetTemp;
                // Check if we passed the target
                float currentSet = startTemp + ((target > startTemp) ? 1 : -1) * ratePerSec * elapsedSec;
                if ((target > startTemp && currentSet >= target) || (target < startTemp && currentSet <= target)) {
                    stepComplete = true;
                }
            }
        }
        else if (step->type == STEP_HOLD_STABLE) {
            // durationSec holds Threshold * 100 (deg/min)
            // Check stability over 5 second window
            if (now - stabilityRefTime >= 5000) {
                float rate = (currentTemp - stabilityRefTemp) / 5.0 * 60.0; // deg/min
                float threshold = (float)step->durationSec / 100.0;
                if (abs(rate) <= threshold) stepComplete = true;
                
                // Reset window
                stabilityRefTemp = currentTemp;
                stabilityRefTime = now;
            }
        }
        else if (step->type == STEP_WAIT_INPUT) {
            // targetTemp holds Pin, durationSec holds State (0/1)
            int pin = (int)step->targetTemp;
            int expected = (int)step->durationSec;
            if (digitalRead(pin) == expected) stepComplete = true;
        }

        if (stepComplete) {
            // Step Complete
            currentStepIndex++;
            if (currentStepIndex >= activeProfile->stepCount) {
                stop(); // Profile Complete
                return 0.0;
            }
            // Setup next step (Advance)
            stepStartTime = now;
            
            // Determine start temp for next step
            // If previous was RAMP/SOAK/HOLD, use its target to ensure continuity.
            // If previous was WAIT_INPUT, use current temp because Pin # is not a temp.
            if (step->type == STEP_WAIT_INPUT) {
                startTemp = currentTemp;
            } else {
                startTemp = step->targetTemp; 
            }
            
            initStep(currentTemp);
            return startTemp;
        }

        // --- Setpoint Calculation ---
        if (step->type == STEP_SOAK_TIME || step->type == STEP_HOLD_STABLE) {
            return step->targetTemp;
        } 
        else if (step->type == STEP_RAMP_TIME) {
            // Linear Interpolation
            float progress = (float)elapsedSec / (float)step->durationSec;
            return startTemp + (step->targetTemp - startTemp) * progress;
        }
        else if (step->type == STEP_RAMP_RATE) {
            float ratePerSec = (float)step->durationSec / 100.0 / 60.0;
            return startTemp + ((step->targetTemp > startTemp) ? 1 : -1) * ratePerSec * elapsedSec;
        }
        else {
            return startTemp; // Wait Input holds previous temp (startTemp)
        }
    }
    
    unsigned long getTimeRemaining() {
        if (!isRunning || !activeProfile) return 0;
        unsigned long now = millis();
        if (isPaused) now = pauseStartTime;
        
        ProfileStep* step = &activeProfile->steps[currentStepIndex];
        if (step->type == STEP_RAMP_TIME || step->type == STEP_SOAK_TIME) {
            unsigned long elapsed = (now - stepStartTime) / 1000;
            long rem = step->durationSec - elapsed;
            return (rem > 0) ? rem : 0;
        }
        return 0; // Indeterminate for other types
    }

private:
    void initStep(float current) {
        ProfileStep* step = &activeProfile->steps[currentStepIndex];
        if (step->type == STEP_HOLD_STABLE) { stabilityRefTemp = current; stabilityRefTime = millis(); }
        if (step->type == STEP_WAIT_INPUT) { pinMode((int)step->targetTemp, INPUT_PULLUP); }
    }
};

#endif
