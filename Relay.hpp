#ifndef RELAY_H
#define RELAY_H

#include <Arduino.h>
#include "HardwareManager.hpp"
#include "PreferenceModel.hpp"
#include "StorageManager.hpp"
#include "TaskQueueManager.hpp"
#include "LoggingTask.hpp"
#include "JsonSerializable.hpp"

extern TaskQueueManager sysQueue;
extern void triggerFeedback();

class Relay : public PreferenceModel, public JsonSerializable {
private:
    HardwareManager* _hardwareManager; // Shared hardware manager for MCP access
    uint8_t _pin;                      // The specific MCP23X17 pin for this relay
    bool _state;                       // Current state of the relay (true = ON, false = OFF)
    bool _isActiveLow;                 // Configuration flag if the physical relay module triggers on LOW
    const char* _storageKey;
    StorageManager* _storage;

public:
    Relay();
    // Constructor: Takes the shared HardwareManager, the pin number, the storage key, and whether the board is Active-Low
    Relay(HardwareManager& hardwareManager, uint8_t pin, const char* storageKey, StorageManager& storage, bool isActiveLow = false);
    void init(HardwareManager& hardwareManager, uint8_t pin, const char* storageKey, StorageManager& storage, bool isActiveLow = false);

    // Initializes the pin mode via the HardwareManager and applies loaded state
    void begin();

    // Turn the relay ON
    void turnOn();

    // Turn the relay OFF
    void turnOff();

    // Inverts the current state of the relay
    void toggle();

    // Returns true if the relay is currently ON
    bool isOn() const;

    // Returns the raw underlying state value
    bool getState() const;

    uint8_t getPin() const { return _pin; }

    String toJson() override;

    bool save(Preferences &prefs) override;
    bool load(Preferences &prefs) override;
};

// Implementation

// Constructor implementation
Relay::Relay()
    : _hardwareManager(nullptr), _pin(0), _state(false), _isActiveLow(false), _storageKey(nullptr), _storage(nullptr) {}

Relay::Relay(HardwareManager& hardwareManager, uint8_t pin, const char* storageKey, StorageManager& storage, bool isActiveLow)
    : _hardwareManager(&hardwareManager), _pin(pin), _state(false), _isActiveLow(isActiveLow), _storageKey(storageKey), _storage(&storage) {}

void Relay::init(HardwareManager& hardwareManager, uint8_t pin, const char* storageKey, StorageManager& storage, bool isActiveLow) {
    _hardwareManager = &hardwareManager;
    _pin = pin;
    _isActiveLow = isActiveLow;
    _storageKey = storageKey;
    _storage = &storage;
}

void Relay::begin() {
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "RELAY", String("Relay begin(pin=") + _pin + ", key=" + (_storageKey ? _storageKey : "null") + ")"));
    
    // 1. Load persisted state now that storage has been initialized, before configuring pin
    if (!load(_storage->prefs())) {
        sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::WARNING, "RELAY", String("Relay load failed for key=") + (_storageKey ? _storageKey : "null")));
    } else {
        sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "RELAY", String("Relay loaded state=") + (_state ? "ON" : "OFF") + " for key=" + _storageKey));
    }

    // 2. Set the pin output value in MCP23X17 register before configuring as OUTPUT
    // This ensures it transitions directly to the correct state when enabled
    uint8_t pinValue = _state ? (_isActiveLow ? LOW : HIGH) : (_isActiveLow ? HIGH : LOW);
    _hardwareManager->writePin(_pin, pinValue);

    // 3. Configure the MCP23X17 pin as an output through the shared hardware manager
    _hardwareManager->setPinMode(_pin, OUTPUT);
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "RELAY", String("Relay applied state=") + (_state ? "ON" : "OFF") + " on pin=" + _pin));
}

void Relay::turnOn() {
    _state = true;
    uint8_t pinValue = _isActiveLow ? LOW : HIGH;
    _hardwareManager->writePin(_pin, pinValue);
    save(_storage->prefs());
    triggerFeedback();
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "RELAY", String("Relay turned ON pin=") + _pin + " key=" + (_storageKey ? _storageKey : "null")));
}

void Relay::turnOff() {
    _state = false;
    uint8_t pinValue = _isActiveLow ? HIGH : LOW;
    _hardwareManager->writePin(_pin, pinValue);
    save(_storage->prefs());
    triggerFeedback();
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "RELAY", String("Relay turned OFF pin=") + _pin + " key=" + (_storageKey ? _storageKey : "null")));
}

void Relay::toggle() {
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "RELAY", String("Relay toggling pin=") + _pin + " current=" + (_state ? "ON" : "OFF")));
    if (_state) {
        turnOff();
    } else {
        turnOn();
    }
}

bool Relay::isOn() const {
    uint8_t pinValue = _hardwareManager->readPin(_pin);
    return _isActiveLow ? (pinValue == LOW) : (pinValue == HIGH);
}

bool Relay::getState() const {
    return isOn();
}

String Relay::toJson() {
    String json = "{";
    json += "\"type\":\"relay\",";
    json += "\"pin\":" + String(_pin) + ",";
    json += "\"key\":\"" + String(_storageKey ? _storageKey : "null") + "\",";
    json += "\"state\":" + String(isOn() ? "true" : "false") + ",";
    json += "\"activeLow\":" + String(_isActiveLow ? "true" : "false");
    json += "}";
    return json;
}

bool Relay::save(Preferences &prefs) {
    if (_storageKey == nullptr) {
        sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::ERROR, "RELAY", "Save failed: missing storage key"));
        return false;
    }
    prefs.putBool(_storageKey, _state);
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "RELAY", String("Relay state saved key=") + _storageKey + " value=" + (_state ? "1" : "0")));
    return true;
}

bool Relay::load(Preferences &prefs) {
    if (_storageKey == nullptr) {
        sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::ERROR, "RELAY", "Load failed: missing storage key"));
        return false;
    }
    _state = prefs.getBool(_storageKey, _state);
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "RELAY", String("Relay state loaded key=") + _storageKey + " value=" + (_state ? "1" : "0")));
    return true;
}

#endif // RELAY_H
