# R4 Thermal Controller Architecture

This project is a sophisticated thermal controller built on an Arduino platform, likely for applications like a reflow oven or a sous-vide cooker.

## Core Functionality

The system's main purpose is to precisely control a heating element (via a Solid State Relay or SSR) to follow a user-defined temperature profile. This is achieved using a PID (Proportional-Integral-Derivative) control loop.

## Architectural Components

1.  **Main Controller (`pid_controller.ino`):**
    *   This is the heart of the sketch, running on an Arduino with WiFi capabilities (like an Uno R4 WiFi or a board with an ESP8266/ESP32).
    *   It orchestrates all other components.
    *   It reads temperature from a connected sensor (supporting both DS18B20 and MAX31856 thermocouple sensors).
    *   It runs a web server to host a user interface and a WebSocket server for real-time communication.
    *   It manages the system's overall state (e.g., `IDLE`, `RUNNING`, `TUNING`).

2.  **PID & SSR Control (`PID_SSR.h`):**
    *   This class implements the PID control algorithm. It takes a target temperature (setpoint) and the current temperature as input and calculates the required power output (0-100%).
    *   It includes an auto-tuning feature to automatically find optimal PID constants (Kp, Ki, Kd).
    *   It manages the SSR output using a time-proportional "windowing" technique to achieve fine-grained power control.

3.  **Profile Engine (`ProfileEngine.h`):**
    *   This class allows you to define multi-step temperature profiles (e.g., "ramp to 150°C over 60s, then soak at 150°C for 90s").
    *   During a run, it calculates the correct temperature setpoint for the PID controller at any given moment based on the active profile.

4.  **Configuration & Persistence (`EEPROM_Manager.h`):**
    *   This component saves all critical settings to the Arduino's non-volatile EEPROM memory so they persist after power cycles.
    *   This includes WiFi credentials, saved temperature profiles, PID constants, and sensor configurations.
    *   It has a CRC check to ensure data integrity and loads default values if the EEPROM is empty or corrupt.

5.  **Web Interface (`index.html` & `web_assets.h`):**
    *   The project hosts a self-contained, single-page web application that serves as the user interface.
    *   This UI connects to the Arduino via WebSockets to display live data (temperature, setpoint, SSR power) and a historical chart.
    *   It allows the user to:
        *   Start, stop, and pause heating profiles.
        *   Manually edit and save PID constants.
        *   Run the PID auto-tuner.
        *   Create, edit, and save multi-step temperature profiles.
        *   Configure system settings like WiFi, sensor type, and safety limits.
    *   The `web_assets.h` file contains the HTML and icon, compressed and stored as C++ byte arrays, which are served directly from the Arduino's memory. The `convert.py` script is used to perform this compression.

In summary, it's a full-stack embedded system where the Arduino acts as both the control hardware and the web server, providing a rich, real-time user experience for thermal process control.
