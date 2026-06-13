#ifndef CLIMATE_SENSOR_H
#define CLIMATE_SENSOR_H

#include <Arduino.h>
#include <DHT.h>
#include "JsonSerializable.hpp"
#include "LoggingTask.hpp"
#include "TaskQueueManager.hpp"
#include "ErrorManager.hpp"

extern TaskQueueManager sysQueue;

class ClimateSensor : public JsonSerializable {
private:
    DHT _dht;
    float _lastTemperature;
    float _lastHumidity;
    bool _isHealthy;
    unsigned long _lastReadTime;
    const unsigned long _readInterval = 2000; 

public:
    ClimateSensor(uint8_t pin, uint8_t type);
    
    void init();
    void update();
    
    float getTemperature() const;
    float getHumidity() const;
    bool isHealthy() const;
    String toJson() override;
};

// Implementation

// Constructor explicitly initialization list mapping
ClimateSensor::ClimateSensor(uint8_t pin, uint8_t type) 
    : _dht(pin, type), _lastTemperature(0.0f), _lastHumidity(0.0f), _isHealthy(false), _lastReadTime(0) {}

// Hardware bus initialization
void ClimateSensor::init() {
    _dht.begin();
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "CLIMATE", String("ClimateSensor init")));
}

// Non-blocking sampling scheduler execution
void ClimateSensor::update() {
    unsigned long currentMillis = millis();
    if (currentMillis - _lastReadTime >= _readInterval || _lastReadTime == 0) {
        _lastReadTime = currentMillis;

        float t = _dht.readTemperature();
        float h = _dht.readHumidity();

        if (isnan(t) || isnan(h)) {
            if (_isHealthy) {
                _isHealthy = false;
                ErrorManager::getInstance().setError(ERROR_DHT_SENSOR);
                sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::WARNING, "CLIMATE", String("ClimateSensor read failed (nan)")));
            }
        } else {
            _lastTemperature = t;
            _lastHumidity = h;
            if (!_isHealthy) {
                _isHealthy = true;
                ErrorManager::getInstance().clearError(ERROR_DHT_SENSOR);
                sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "CLIMATE", String("ClimateSensor recovered and online")));
            }
        }
    }
}

// Immutable getters
float ClimateSensor::getTemperature() const { 
    return _lastTemperature; 
}

float ClimateSensor::getHumidity() const { 
    return _lastHumidity; 
}

bool ClimateSensor::isHealthy() const { 
    return _isHealthy; 
}

String ClimateSensor::toJson() {
    String json = "{";
    json += "\"type\":\"climateSensor\",";
    json += "\"temperature\":" + String(_lastTemperature, 2) + ",";
    json += "\"humidity\":" + String(_lastHumidity, 2) + ",";
    json += "\"healthy\":" + String(_isHealthy ? "true" : "false");
    json += "}";
    return json;
}

#endif
