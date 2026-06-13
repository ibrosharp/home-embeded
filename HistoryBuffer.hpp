#ifndef HISTORY_BUFFER_HPP
#define HISTORY_BUFFER_HPP

#include <Arduino.h>
#include "JsonSerializable.hpp"

const int HISTORY_MAX_POINTS = 60; // 60 minutes of history

struct HistoryPoint {
    float temperature;
    float humidity;
    float light;
    unsigned long timestamp; // millis()
};

class HistoryBuffer : public JsonSerializable {
private:
    HistoryPoint _points[HISTORY_MAX_POINTS];
    int _head;
    int _count;
    unsigned long _lastSampleTime;

public:
    HistoryBuffer() : _head(0), _count(0), _lastSampleTime(0) {
        for (int i = 0; i < HISTORY_MAX_POINTS; i++) {
            _points[i].temperature = 0;
            _points[i].humidity = 0;
            _points[i].light = 0;
            _points[i].timestamp = 0;
        }
    }

    void addPoint(float temp, float hum, float light) {
        _points[_head].temperature = temp;
        _points[_head].humidity = hum;
        _points[_head].light = light;
        _points[_head].timestamp = millis();

        _head = (_head + 1) % HISTORY_MAX_POINTS;
        if (_count < HISTORY_MAX_POINTS) {
            _count++;
        }
        _lastSampleTime = millis();
    }

    unsigned long getLastSampleTime() const {
        return _lastSampleTime;
    }

    String toJson() override {
        String json = "{\"points\":[";
        
        int startIdx = (_count < HISTORY_MAX_POINTS) ? 0 : _head;
        for (int i = 0; i < _count; i++) {
            int idx = (startIdx + i) % HISTORY_MAX_POINTS;
            json += "{";
            json += "\"t\":" + String(_points[idx].temperature, 1) + ",";
            json += "\"h\":" + String(_points[idx].humidity, 1) + ",";
            json += "\"l\":" + String(_points[idx].light, 1);
            json += "}";
            if (i < _count - 1) json += ",";
        }
        
        json += "]}";
        return json;
    }
};

#endif // HISTORY_BUFFER_HPP
