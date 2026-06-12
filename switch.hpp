#ifndef SWITCH_H
#define SWITCH_H

#include <Arduino.h>
#include "HardwareManager.hpp"
#include "Relay.hpp"
#include "PreferenceModel.hpp"
#include "StorageManager.hpp"
#include "TaskQueueManager.hpp"
#include "LoggingTask.hpp"
#include "JsonSerializable.hpp"

extern TaskQueueManager sysQueue;
extern void triggerFeedback();

class Switch : public PreferenceModel, public JsonSerializable {
private:
    HardwareManager* _hardwareManager;
    uint8_t _pin;
    uint8_t _state;
    uint8_t _lastState;
    uint8_t _lastPinState;

    unsigned long _lastDebounceTime;
    unsigned long _debounceDelay;

    Relay* _relay;
    const char* _storageKey;
    StorageManager* _storage;

    uint8_t _loadedRelayPin;
    bool _hasLoadedRelayPin;

public:
    Switch();
    Switch(HardwareManager& hardwareManager, uint8_t pin, const char* storageKey, StorageManager& storage, unsigned long debounceDelay = 50);
    void init(HardwareManager& hardwareManager, uint8_t pin, const char* storageKey, StorageManager& storage, unsigned long debounceDelay = 50);
    void begin();
    void update();
    bool isPressed() const;
    bool rose() const;
    bool fell() const;
    uint8_t getState() const;
    uint8_t getPin() const { return _pin; }
    void setState(uint8_t newState);

    bool hasLoadedRelayPin() const { return _hasLoadedRelayPin; }
    uint8_t getLoadedRelayPin() const { return _loadedRelayPin; }

    void attachRelay(Relay* relay, bool saveToStorage = true);
    bool save(Preferences &prefs) override;
    bool load(Preferences &prefs) override;
    String toJson() override;
};

// Implementation

Switch::Switch()
    : _hardwareManager(nullptr), _pin(0), _state(HIGH), _lastState(HIGH), _lastPinState(HIGH),
      _lastDebounceTime(0), _debounceDelay(50), _relay(nullptr), _storageKey(nullptr), _storage(nullptr),
      _loadedRelayPin(255), _hasLoadedRelayPin(false) {}

Switch::Switch(HardwareManager& hardwareManager, uint8_t pin, const char* storageKey, StorageManager& storage, unsigned long debounceDelay)
    : _hardwareManager(&hardwareManager), _pin(pin), _state(HIGH), _lastState(HIGH), _lastPinState(HIGH),
      _lastDebounceTime(0), _debounceDelay(debounceDelay), _relay(nullptr), _storageKey(storageKey), _storage(&storage),
      _loadedRelayPin(255), _hasLoadedRelayPin(false) {}

void Switch::init(HardwareManager& hardwareManager, uint8_t pin, const char* storageKey, StorageManager& storage, unsigned long debounceDelay) {
    _hardwareManager = &hardwareManager;
    _pin = pin;
    _debounceDelay = debounceDelay;
    _relay = nullptr;
    _storageKey = storageKey;
    _storage = &storage;
    _loadedRelayPin = 255;
    _hasLoadedRelayPin = false;
}

void Switch::begin() {
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "SWITCH", String("Switch begin(pin=") + _pin + ", key=" + (_storageKey ? _storageKey : "null") + ")"));
    // Configure the MCP switch pin as INPUT; the MCP board already uses
    // pull resistors so the line is stable and never read as floating.
    _hardwareManager->setPinMode(_pin, INPUT);
    if (!load(_storage->prefs())) {
        sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::WARNING, "SWITCH", String("Switch load failed for key=") + (_storageKey ? _storageKey : "null")));
    } else {
        sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "SWITCH", String("Switch loaded state=") + _state + " key=" + (_storageKey ? _storageKey : "null")));
    }
    _lastState = _state;
    _lastPinState = _hardwareManager->readPin(_pin);
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "SWITCH", String("Switch initial pin state=") + _lastPinState + " internal state=" + _state));
}

void Switch::attachRelay(Relay* relay, bool saveToStorage) {
    _relay = relay;
    if (saveToStorage && _storage != nullptr) {
        save(_storage->prefs());
    }
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "SWITCH", String("Attached relay pointer=") + String((uintptr_t)relay) + " to switch pin=" + _pin));
}

void Switch::setState(uint8_t newState) {
    if (_state != newState) {
        _lastState = _state;
        _state = newState;
        save(_storage->prefs());
        triggerFeedback();
        if (_relay != nullptr) {
            _relay->toggle();
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "SWITCH", String("API set switch pin=") + _pin + " state=" + _state + " (toggled relay)"));
        } else {
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "SWITCH", String("API set switch pin=") + _pin + " state=" + _state));
        }
    }
}

void Switch::update() {
    uint8_t reading = _hardwareManager->readPin(_pin);

    if (reading != _lastPinState) {
        if (_lastDebounceTime == 0) {
            _lastDebounceTime = millis();
        }

        if ((millis() - _lastDebounceTime) > _debounceDelay) {
            _lastPinState = reading;
            _lastDebounceTime = 0;
            _lastState = _state;
            _state = (_state == HIGH) ? LOW : HIGH;
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "SWITCH", String("Switch pin changed pin=") + _pin + " newLogicalState=" + _state + " physicalPin=" + reading));
            triggerFeedback();

            if (_relay != nullptr) {
                _relay->toggle();
                sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "SWITCH", String("Toggled relay for switch pin=") + _pin));
            } else {
                sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::WARNING, "SWITCH", String("Switch pin changed but no relay attached pin=") + _pin));
            }

            save(_storage->prefs());
        }
    } else {
        _lastDebounceTime = 0;
    }
}

bool Switch::save(Preferences &prefs) {
    if (_storageKey == nullptr) {
        sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::ERROR, "SWITCH", "Save failed: missing storage key"));
        return false;
    }
    prefs.putUChar(_storageKey, _state);

    char relayKey[24];
    snprintf(relayKey, sizeof(relayKey), "%s_r", _storageKey);
    uint8_t attachedPin = (_relay != nullptr) ? _relay->getPin() : 255;
    prefs.putUChar(relayKey, attachedPin);

    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "SWITCH", String("Switch state saved key=") + _storageKey + " value=" + _state + " attachedRelayPin=" + attachedPin));
    return true;
}

bool Switch::load(Preferences &prefs) {
    if (_storageKey == nullptr) {
        sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::ERROR, "SWITCH", "Load failed: missing storage key"));
        return false;
    }
    _state = prefs.getUChar(_storageKey, _state);
    _lastState = _state;

    char relayKey[24];
    snprintf(relayKey, sizeof(relayKey), "%s_r", _storageKey);
    if (prefs.isKey(relayKey)) {
        _loadedRelayPin = prefs.getUChar(relayKey, 255);
        _hasLoadedRelayPin = true;
    }

    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "SWITCH", String("Switch state loaded key=") + _storageKey + " value=" + _state + " loadedRelayPin=" + (_hasLoadedRelayPin ? String(_loadedRelayPin) : "none")));
    return true;
}

bool Switch::isPressed() const {
    return (_state == LOW);
}

bool Switch::rose() const {
    return (_lastState == HIGH && _state == LOW);
}

bool Switch::fell() const {
    return (_lastState == LOW && _state == HIGH);
}

uint8_t Switch::getState() const {
    return _state;
}

String Switch::toJson() {
    String json = "{";
    json += "\"type\":\"switch\",";
    json += "\"pin\":" + String(_pin) + ",";
    json += "\"key\":\"" + String(_storageKey ? _storageKey : "null") + "\",";
    json += "\"state\":" + String(_state) + ",";
    json += "\"lastState\":" + String(_lastState) + ",";
    json += "\"debounceDelay\":" + String(_debounceDelay) + ",";
    if (_relay != nullptr) {
        json += "\"hasLoad\":true,";
        json += "\"load\":" + _relay->toJson();
    } else {
        json += "\"hasLoad\":false";
    }
    json += "}";
    return json;
}

#endif // SWITCH_H
