#ifndef CLIMATE_AUTOMATION_HPP
#define CLIMATE_AUTOMATION_HPP

#include <Arduino.h>
#include "PreferenceModel.hpp"
#include "JsonSerializable.hpp"
#include "StorageManager.hpp"
#include "TaskQueueManager.hpp"
#include "LoggingTask.hpp"

extern TaskQueueManager sysQueue;

class Room;
class IRController;

class ClimateAutomation : public PreferenceModel, public JsonSerializable {
private:
    bool _enabled;
    float _targetTemp;
    int _irSlotPowerOn;
    int _irSlotPowerOff;
    bool _triggered; // true if AC is currently running (temp was > target)

    const char* _storageKey;
    StorageManager* _storage;

public:
    ClimateAutomation(const char* storageKey, StorageManager& storage)
        : _enabled(false), _targetTemp(24.0f), _irSlotPowerOn(-1), _irSlotPowerOff(-1), _triggered(false),
          _storageKey(storageKey), _storage(&storage) {}

    void begin() {
        if (!load(_storage->prefs())) {
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::WARNING, "AUTO", "ClimateAutomation load failed or defaults used"));
        } else {
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "AUTO", String("ClimateAutomation loaded. Enabled: ") + _enabled + " Target: " + _targetTemp));
        }
    }

    void setConfig(bool enabled, float targetTemp, int irSlotPowerOn, int irSlotPowerOff) {
        _enabled = enabled;
        _targetTemp = targetTemp;
        _irSlotPowerOn = irSlotPowerOn;
        _irSlotPowerOff = irSlotPowerOff;
        _triggered = false; // Reset state on config change
        save(_storage->prefs());
    }

    void evaluate(float currentTemp, Room& room);

    bool save(Preferences &prefs) override {
        if (_storageKey == nullptr) return false;
        
        prefs.putBool((String(_storageKey) + "_en").c_str(), _enabled);
        prefs.putFloat((String(_storageKey) + "_tt").c_str(), _targetTemp);
        prefs.putInt((String(_storageKey) + "_irOn").c_str(), _irSlotPowerOn);
        prefs.putInt((String(_storageKey) + "_irOff").c_str(), _irSlotPowerOff);
        
        return true;
    }

    bool load(Preferences &prefs) override {
        if (_storageKey == nullptr) return false;
        
        if (!prefs.isKey((String(_storageKey) + "_en").c_str())) return false;
        
        _enabled = prefs.getBool((String(_storageKey) + "_en").c_str(), false);
        _targetTemp = prefs.getFloat((String(_storageKey) + "_tt").c_str(), 24.0f);
        _irSlotPowerOn = prefs.getInt((String(_storageKey) + "_irOn").c_str(), -1);
        _irSlotPowerOff = prefs.getInt((String(_storageKey) + "_irOff").c_str(), -1);
        
        return true;
    }

    String toJson() override {
        String json = "{";
        json += "\"enabled\":" + String(_enabled ? "true" : "false") + ",";
        json += "\"targetTemp\":" + String(_targetTemp) + ",";
        json += "\"irSlotPowerOn\":" + String(_irSlotPowerOn) + ",";
        json += "\"irSlotPowerOff\":" + String(_irSlotPowerOff);
        json += "}";
        return json;
    }
};

#endif // CLIMATE_AUTOMATION_HPP
