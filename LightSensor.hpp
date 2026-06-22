#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <Arduino.h>
#include "JsonSerializable.hpp"
#include "LoggingTask.hpp"
#include "TaskQueueManager.hpp"

extern TaskQueueManager sysQueue;
extern volatile bool globalStateChanged;

class LightSensor : public JsonSerializable {
private:
    uint8_t _pin;
    int _lastRawValue;

public:
    LightSensor(uint8_t pin);
    
    void init();
    void update(); // Reads and updates the internal state cache
    
    int getRawValue() const;
    float getPercentage() const; // Returns 0.0 (completely dark) to 100.0 (maximum light)
    String toJson() override;
};

// Implementation

LightSensor::LightSensor(uint8_t pin) : _pin(pin), _lastRawValue(0) {}

void LightSensor::init() {
    // Configure the pin as an input. The ESP32 ADC handles the conversion.
    pinMode(_pin, INPUT);
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "LIGHT", String("LightSensor init pin=") + _pin));
}

void LightSensor::update() {
    // Read the 12-bit ADC value (returns a range from 0 to 4095 on an ESP32)
    int newRaw = analogRead(_pin);
    if (abs(newRaw - _lastRawValue) > 50) { // Avoid flooding Websocket with tiny ADC noise (50/4095 ~ 1.2%)
        globalStateChanged = true;
    }
    _lastRawValue = newRaw;
}

int LightSensor::getRawValue() const {
    return _lastRawValue;
}

float LightSensor::getPercentage() const {
    // Map the 12-bit ADC value (0-4095) to a clean 0-100% value
    // If your LDR circuit is inverted (higher voltage means darker), swap 0 and 100
    float percentage = (_lastRawValue / 4095.0f) * 100.0f;
    
    if (percentage < 0.0f) return 0.0f;
    if (percentage > 100.0f) return 100.0f;
    return percentage;
}

String LightSensor::toJson() {
    String json = "{";
    json += "\"type\":\"lightSensor\",";
    json += "\"pin\":" + String(_pin) + ",";
    json += "\"rawValue\":" + String(_lastRawValue) + ",";
    json += "\"percentage\":" + String(getPercentage(), 2);
    json += "}";
    return json;
}

#endif
