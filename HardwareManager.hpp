#ifndef HARDWARE_MANAGER_HPP
#define HARDWARE_MANAGER_HPP

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>


#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class HardwareManager {
private:
    Adafruit_MCP23X17 _mcp;
    SemaphoreHandle_t _i2cMutex;
    bool _isInitialized;

    // Private constructor for Singleton implementation
    HardwareManager() : _i2cMutex(NULL), _isInitialized(false) {}
    HardwareManager(const HardwareManager&) = delete;
    HardwareManager& operator=(const HardwareManager&) = delete;

public:
    static HardwareManager& getInstance() {
        static HardwareManager instance;
        return instance;
    }

    // Configures and boots up the physical standard Wire I2C bus and MCP23X17
    bool begin(uint8_t sdaPin = 1, uint8_t sclPin = 3, uint8_t address = 0x20) {
        if (_isInitialized) return true;

        _i2cMutex = xSemaphoreCreateMutex();
        if (_i2cMutex == NULL) return false;

        // Set a timeout for the Wire bus so it doesn't freeze forever if hardware is missing
        // 50ms is plenty for standard I2C
        Wire.begin(sdaPin, sclPin, 100000);
        Wire.setTimeOut(50); 

        Serial.println("[HW] Checking for MCP23X17...");
        if (!_mcp.begin_I2C(address, &Wire)) {
            Serial.println("[ERROR] MCP23X17 not detected on I2C bus! Proceeding in headless mode.");
            // Return false so the rest of your system knows the hardware isn't real
            _isInitialized = false; 
            return false; 
        }

        _mcp.setupInterrupts(true, false, LOW);
        _isInitialized = true;
        return true;
    }
    // bool begin(uint8_t sdaPin = 1, uint8_t sclPin = 3, uint8_t address = 0x20) {
    //     if (_isInitialized) return true;

    //     // 1. Create a FreeRTOS Mutex to prevent multi-task data collisions on the Wire bus
    //     _i2cMutex = xSemaphoreCreateMutex();
    //     if (_i2cMutex == NULL) return false;

    //     // 2. Initialize the standard Arduino 'Wire' bus with your explicit pins
    //     // On the ESP32, Wire.begin() takes (SDA, SCL, frequency)
    //     if (!Wire.begin(sdaPin, sclPin, 100000)) { 
    //         return false;
    //     }

    //     // 3. Mount the Adafruit library directly onto the standard &Wire reference
    //     if (!_mcp.begin_I2C(address, &Wire)) {
    //         return false; 
    //     }

    //     // 4. Optimize the MCP23X17 internal interrupt registers
    //     // true = mirrors INTA and INTB together; false = open-drain output; LOW = active-low alert
    //     _mcp.setupInterrupts(true, false, LOW);

    //     _isInitialized = true;
    //     return true;
    // }

    // Thread-safe wrapper to switch an output pin (e.g., Relays)
    void writePin(uint8_t pin, uint8_t state) {
        if (!_isInitialized) return;

        if (xSemaphoreTake(_i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            _mcp.digitalWrite(pin, state);
            xSemaphoreGive(_i2cMutex); 
        }
    }

    // Thread-safe wrapper to read an input pin (e.g., Switches)
    uint8_t readPin(uint8_t pin) {
        if (!_isInitialized) return HIGH;

        uint8_t state = HIGH;
        if (xSemaphoreTake(_i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            state = _mcp.digitalRead(pin);
            xSemaphoreGive(_i2cMutex);
        }
        return state;
    }

    // Thread-safe wrapper to set internal pin modes during boot setup
    void setPinMode(uint8_t pin, uint8_t mode) {
        if (!_isInitialized) return;

        if (xSemaphoreTake(_i2cMutex, portMAX_DELAY) == pdTRUE) {
            _mcp.pinMode(pin, mode);
            xSemaphoreGive(_i2cMutex);
        }
    }

    // Clear the hardware interrupt bit registers and read both ports instantly
    uint16_t captureAndClearInterrupts() {
        if (!_isInitialized) return 0xFFFF;

        uint16_t states = 0xFFFF;
        if (xSemaphoreTake(_i2cMutex, portMAX_DELAY) == pdTRUE) {
            _mcp.getLastInterruptPin(); // Clear hardware INT state latch
            states = _mcp.readGPIOAB(); // Pull all 16 pins simultaneously
            xSemaphoreGive(_i2cMutex);
        }
        return states;
    }
};

#endif