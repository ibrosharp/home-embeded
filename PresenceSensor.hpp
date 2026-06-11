#ifndef PRESENCE_SENSOR_H
#define PRESENCE_SENSOR_H

#include <Arduino.h>
#include "JsonSerializable.hpp"
#include "LoggingTask.hpp"
#include "TaskQueueManager.hpp"

extern TaskQueueManager sysQueue;

class PresenceSensor : public JsonSerializable {
private:
    uint8_t _pin;
    bool _lastMotion;
    uint16_t _falseStableCount;
    static const uint16_t FALSE_STABLE_THRESHOLD = 100;

public:
    PresenceSensor(uint8_t pin);
    void init();
    void update(); // Samples and caches the current PIR state
    bool isMotionDetected() const;
    String toJson() override {
        String json = "{";
        json += "\"type\":\"presenceSensor\",";
        json += "\"pin\":" + String(_pin) + ",";
        json += "\"motion\":" + String(_lastMotion ? "true" : "false");
        json += "}";
        return json;
    }
};

// Implementation

PresenceSensor::PresenceSensor(uint8_t pin)
    : _pin(pin), _lastMotion(false), _falseStableCount(0) {}

void PresenceSensor::init() {
    pinMode(_pin, INPUT_PULLDOWN); 
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "PRESENCE", String("PresenceSensor init pin=") + _pin));
}

// Snapshot the PIR pin into the cache
void PresenceSensor::update() {
    bool newMotion = (digitalRead(_pin) == HIGH);

    if (_lastMotion) {
        if (!newMotion) {
            _falseStableCount++;
            if (_falseStableCount >= FALSE_STABLE_THRESHOLD) {
                _lastMotion = false;
                _falseStableCount = 0;
                sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "PRESENCE", String("Motion=false after stable false period pin=") + _pin));
            }
        } else {
            if (_falseStableCount > 0) {
                _falseStableCount = 0;
            }
        }
    } else {
        if (newMotion) {
            _lastMotion = true;
            _falseStableCount = 0;
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "PRESENCE", String("Motion=true pin=") + _pin));
        }
    }
}

bool PresenceSensor::isMotionDetected() const {
    return _lastMotion;
}

#endif
