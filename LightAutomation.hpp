#ifndef LIGHT_AUTOMATION_HPP
#define LIGHT_AUTOMATION_HPP

#include <Arduino.h>
#include <vector>
#include "PreferenceModel.hpp"
#include "JsonSerializable.hpp"
#include "StorageManager.hpp"
#include "TaskQueueManager.hpp"
#include "LoggingTask.hpp"

extern TaskQueueManager sysQueue;

class Room;
class Relay;

class LightAutomation : public PreferenceModel, public JsonSerializable {
private:
    bool _enabled;
    float _threshold;
    std::vector<uint8_t> _loadPins;
    bool _triggered;
    
    // Auto-off settings
    bool _autoOffEnabled;
    uint16_t _autoOffTimeoutSeconds;
    unsigned long _lastMotionTime;

    const char* _storageKey;
    StorageManager* _storage;

public:
    LightAutomation(const char* storageKey, StorageManager& storage)
        : _enabled(false), _threshold(2.0), _triggered(false),
          _autoOffEnabled(false), _autoOffTimeoutSeconds(300), _lastMotionTime(0),
          _storageKey(storageKey), _storage(&storage) {}

    void begin() {
        if (!load(_storage->prefs())) {
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::WARNING, "AUTO", "LightAutomation load failed or defaults used"));
        } else {
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "AUTO", String("LightAutomation loaded. Enabled: ") + _enabled + " Threshold: " + _threshold));
        }
    }

    void setConfig(bool enabled, float threshold, const std::vector<uint8_t>& loadPins, bool autoOffEnabled = false, uint16_t autoOffTimeoutSeconds = 300) {
        _enabled = enabled;
        _threshold = threshold;
        _loadPins = loadPins;
        _autoOffEnabled = autoOffEnabled;
        _autoOffTimeoutSeconds = autoOffTimeoutSeconds;
        _triggered = false; // reset trigger state on config change
        save(_storage->prefs());
    }

    void evaluate(float currentLightPercentage, Room& room);

    bool save(Preferences &prefs) override {
        if (_storageKey == nullptr) return false;
        
        prefs.putBool((String(_storageKey) + "_en").c_str(), _enabled);
        prefs.putFloat((String(_storageKey) + "_th").c_str(), _threshold);
        prefs.putBool((String(_storageKey) + "_aoEn").c_str(), _autoOffEnabled);
        prefs.putUShort((String(_storageKey) + "_aoTm").c_str(), _autoOffTimeoutSeconds);
        
        // Save load pins as comma-separated string
        String pinsStr = "";
        for (size_t i = 0; i < _loadPins.size(); ++i) {
            pinsStr += String(_loadPins[i]);
            if (i < _loadPins.size() - 1) pinsStr += ",";
        }
        prefs.putString((String(_storageKey) + "_pins").c_str(), pinsStr);
        
        return true;
    }

    bool load(Preferences &prefs) override {
        if (_storageKey == nullptr) return false;
        
        if (!prefs.isKey((String(_storageKey) + "_en").c_str())) return false;
        
        _enabled = prefs.getBool((String(_storageKey) + "_en").c_str(), false);
        _threshold = prefs.getFloat((String(_storageKey) + "_th").c_str(), 2.0f);
        _autoOffEnabled = prefs.getBool((String(_storageKey) + "_aoEn").c_str(), false);
        _autoOffTimeoutSeconds = prefs.getUShort((String(_storageKey) + "_aoTm").c_str(), 300);
        
        String pinsStr = prefs.getString((String(_storageKey) + "_pins").c_str(), "");
        _loadPins.clear();
        
        if (pinsStr.length() > 0) {
            int startIndex = 0;
            int commaIndex = pinsStr.indexOf(',');
            while (commaIndex != -1) {
                _loadPins.push_back((uint8_t)pinsStr.substring(startIndex, commaIndex).toInt());
                startIndex = commaIndex + 1;
                commaIndex = pinsStr.indexOf(',', startIndex);
            }
            if (startIndex < pinsStr.length()) {
                _loadPins.push_back((uint8_t)pinsStr.substring(startIndex).toInt());
            }
        }
        
        return true;
    }

    String toJson() override {
        String json = "{";
        json += "\"enabled\":" + String(_enabled ? "true" : "false") + ",";
        json += "\"threshold\":" + String(_threshold) + ",";
        json += "\"autoOffEnabled\":" + String(_autoOffEnabled ? "true" : "false") + ",";
        json += "\"autoOffTimeoutSeconds\":" + String(_autoOffTimeoutSeconds) + ",";
        json += "\"loadPins\":[";
        for (size_t i = 0; i < _loadPins.size(); ++i) {
            json += String(_loadPins[i]);
            if (i < _loadPins.size() - 1) json += ",";
        }
        json += "]";
        json += "}";
        return json;
    }
};

#endif // LIGHT_AUTOMATION_HPP
