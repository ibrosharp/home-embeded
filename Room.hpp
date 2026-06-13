#ifndef ROOM_H
#define ROOM_H

#include <Arduino.h>
#include "IRController.hpp"
#include "PresenceSensor.hpp"
#include "ClimateSensor.hpp"
#include "LightSensor.hpp"
#include "LightAutomation.hpp"
#include "ClimateAutomation.hpp"
#include "HistoryBuffer.hpp"
#include "JsonSerializable.hpp"
#include "Relay.hpp"
#include "switch.hpp"
#include "Setting.hpp"
#include "SceneManager.hpp"
#include "LoggingTask.hpp"
#include "TaskQueueManager.hpp"

extern TaskQueueManager sysQueue;


const int MAX_RELAYS = 4;
const int MAX_SWITCHES = 12;

class Room : public JsonSerializable {
private:
    Relay* _relays[MAX_RELAYS];
    int _relayCount;

    Switch* _switches[MAX_SWITCHES];
    int _switchCount;

    PresenceSensor* _presenceSensor;
    ClimateSensor* _climateSensor;
    IRController* _irController;
    LightSensor* _lightSensor;
    LightAutomation* _lightAutomation;
    ClimateAutomation* _climateAutomation;
    HistoryBuffer* _historyBuffer;
    SceneManager* _sceneManager;
    Setting* _settings;
    volatile bool _feedbackPending;

    Room();
    Room(const Room&) = delete;
    Room& operator=(const Room&) = delete;

public:
    static Room& getInstance();

    // Peripheral Registration Methods
    bool registerRelay(Relay* relayInstance);
    bool registerSwitch(Switch* switchInstance);
    void registerPresenceSensor(PresenceSensor& sensorInstance);
    void registerClimateSensor(ClimateSensor& sensorInstance);
    void registerIRController(IRController& irInstance);
    void registerLightSensor(LightSensor& sensorInstance);
    void registerLightAutomation(LightAutomation& automationInstance);
    void registerClimateAutomation(ClimateAutomation& automationInstance);
    void registerHistoryBuffer(HistoryBuffer& historyInstance);
    void registerSceneManager(SceneManager& sceneManagerInstance);
    void registerSetting(Setting& settingInstance);
    void registerSettings(Setting& settingInstance);

    // Getters
    Relay* getRelay(int index) const;
    Switch* getSwitch(int index) const;
    PresenceSensor* getPresenceSensor() const;
    ClimateSensor* getClimateSensor() const;
    IRController* getIRController() const;
    LightSensor* getLightSensor() const;
    LightAutomation* getLightAutomation() const;
    ClimateAutomation* getClimateAutomation() const;
    HistoryBuffer* getHistoryBuffer() const;
    SceneManager* getSceneManager() const;
    Setting* getSetting() const;
    Setting* getSettings() const;
    Switch* findSwitchByPin(uint8_t pin) const;
    Relay* findRelayByPin(uint8_t pin) const;

    void triggerFeedback() {
        if (_settings && _settings->isLedFeedbackEnabled()) {
            _feedbackPending = true;
        }
    }

    bool checkAndClearFeedback() {
        bool pending = _feedbackPending;
        _feedbackPending = false;
        return pending;
    }

    String toJson() override;
};

// ==========================================
//               IMPLEMENTATION
// ==========================================

inline Room::Room() : 
    _relayCount(0), 
    _switchCount(0), 
    _presenceSensor(nullptr), 
    _climateSensor(nullptr),
    _irController(nullptr),
    _lightSensor(nullptr),
    _lightAutomation(nullptr),
    _climateAutomation(nullptr),
    _historyBuffer(nullptr),
    _sceneManager(nullptr),
    _settings(nullptr),
    _feedbackPending(false)
{
    for (int i = 0; i < MAX_RELAYS; i++) _relays[i] = nullptr;
    for (int i = 0; i < MAX_SWITCHES; i++) _switches[i] = nullptr;
}

inline Room& Room::getInstance() {
    static Room instance; 
    return instance;
}

inline bool Room::registerRelay(Relay* relayInstance) {
    if (_relayCount >= MAX_RELAYS || relayInstance == nullptr) return false;
    _relays[_relayCount++] = relayInstance;
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "ROOM", String("Registered relay pin=") + relayInstance->getPin()));
    return true;
}

inline bool Room::registerSwitch(Switch* switchInstance) {
    if (_switchCount >= MAX_SWITCHES || switchInstance == nullptr) return false;
    _switches[_switchCount++] = switchInstance;
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "ROOM", String("Registered switch pin=") + switchInstance->getPin()));
    return true;
}

inline void Room::registerPresenceSensor(PresenceSensor& sensorInstance) {
    _presenceSensor = &sensorInstance;
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "ROOM", String("Registered presence sensor pin=") + sensorInstance.isMotionDetected()));
}

inline void Room::registerClimateSensor(ClimateSensor& sensorInstance) {
    _climateSensor = &sensorInstance;
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "ROOM", String("Registered climate sensor")));
}

inline void Room::registerIRController(IRController& irInstance) {
    _irController = &irInstance;
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "ROOM", String("Registered IR controller")));
}

inline void Room::registerLightSensor(LightSensor& sensorInstance) {
    _lightSensor = &sensorInstance;
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "ROOM", String("Registered light sensor pin=") + sensorInstance.getRawValue()));
}

inline void Room::registerLightAutomation(LightAutomation& automationInstance) {
    _lightAutomation = &automationInstance;
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "ROOM", "Registered light automation"));
}

inline void Room::registerClimateAutomation(ClimateAutomation& automationInstance) {
    _climateAutomation = &automationInstance;
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "ROOM", "Registered climate automation"));
}

inline void Room::registerHistoryBuffer(HistoryBuffer& historyInstance) {
    _historyBuffer = &historyInstance;
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "ROOM", "Registered history buffer"));
}

inline void Room::registerSceneManager(SceneManager& sceneManagerInstance) {
    _sceneManager = &sceneManagerInstance;
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "ROOM", "Registered scene manager"));
}

inline void Room::registerSetting(Setting& settingInstance) {
    _settings = &settingInstance;
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "ROOM", String("Registered setting")));
}

inline void Room::registerSettings(Setting& settingInstance) {
    _settings = &settingInstance;
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "ROOM", String("Registered settings")));
}

inline Relay* Room::getRelay(int index) const { return (index >= 0 && index < _relayCount) ? _relays[index] : nullptr; }
inline Switch* Room::getSwitch(int index) const { return (index >= 0 && index < _switchCount) ? _switches[index] : nullptr; }
inline PresenceSensor* Room::getPresenceSensor() const { return _presenceSensor; }
inline ClimateSensor* Room::getClimateSensor() const { return _climateSensor; }
inline IRController* Room::getIRController() const { return _irController; }
inline LightSensor* Room::getLightSensor() const { return _lightSensor; }
inline LightAutomation* Room::getLightAutomation() const { return _lightAutomation; }
inline ClimateAutomation* Room::getClimateAutomation() const { return _climateAutomation; }
inline HistoryBuffer* Room::getHistoryBuffer() const { return _historyBuffer; }
inline SceneManager* Room::getSceneManager() const { return _sceneManager; }
inline Setting* Room::getSetting() const { return _settings; }
inline Setting* Room::getSettings() const { return _settings; }

inline Switch* Room::findSwitchByPin(uint8_t pin) const {
    for (int i = 0; i < _switchCount; i++) {
        if (_switches[i] != nullptr && _switches[i]->getPin() == pin) {
            return _switches[i];
        }
    }
    return nullptr;
}

inline Relay* Room::findRelayByPin(uint8_t pin) const {
    for (int i = 0; i < _relayCount; i++) {
        if (_relays[i] != nullptr && _relays[i]->getPin() == pin) {
            return _relays[i];
        }
    }
    return nullptr;
}

inline String Room::toJson() {
    String json = "{\"type\":\"room\",\"loads\":[";
    for (int i = 0; i < _relayCount; i++) {
        if (i > 0) json += ",";
        if (_relays[i] != nullptr) json += _relays[i]->toJson();
    }
    json += "],\"switches\":[";
    for (int i = 0; i < _switchCount; i++) {
        if (i > 0) json += ",";
        if (_switches[i] != nullptr) json += _switches[i]->toJson();
    }
    json += "],\"sensors\":{";
    bool firstSensor = true;
    if (_climateSensor != nullptr) {
        if (!firstSensor) json += ",";
        json += "\"climate\":" + _climateSensor->toJson();
        firstSensor = false;
    }
    if (_presenceSensor != nullptr) {
        if (!firstSensor) json += ",";
        json += "\"presence\":" + _presenceSensor->toJson();
        firstSensor = false;
    }
    if (_lightSensor != nullptr) {
        if (!firstSensor) json += ",";
        json += "\"light\":" + _lightSensor->toJson();
        firstSensor = false;
    }
    json += "},\"controllers\":{";
    if (_irController != nullptr) json += "\"ir\":" + _irController->toJson();
    json += "}";
    if (_settings != nullptr) {
        json += ",\"settings\":" + _settings->toJson();
    }
    if (_sceneManager != nullptr) {
        json += ",\"scenes\":" + _sceneManager->toJson();
    }
    json += "}";
    return json;
}

inline void LightAutomation::evaluate(float currentLightPercentage, Room& room) {
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 10000) {
        lastLog = millis();
        sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "AUTO", String("Eval: en=") + _enabled + " light=" + currentLightPercentage + " thresh=" + _threshold + " trig=" + _triggered));
    }

    if (!_enabled) {
        _triggered = false;
        return;
    }

    bool roomEmpty = false;

    // Presence & Auto-Off Logic
    PresenceSensor* presence = room.getPresenceSensor();
    if (presence != nullptr) {
        if (presence->isMotionDetected()) {
            _lastMotionTime = millis();
        } else {
            if (millis() - _lastMotionTime > (_autoOffTimeoutSeconds * 1000UL)) {
                roomEmpty = true;
                
                if (_autoOffEnabled) {
                    bool anyTurnedOff = false;
                    for (uint8_t pin : _loadPins) {
                        Relay* relay = room.findRelayByPin(pin);
                        if (relay != nullptr && relay->isOn()) {
                            relay->turnOff();
                            anyTurnedOff = true;
                            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "AUTO", String("Auto-off: Turned OFF load pin ") + pin));
                        }
                    }
                    if (anyTurnedOff) {
                        _lastMotionTime = millis(); // Prevent spamming turnOff
                    }
                }
                
                // Reset trigger so it can work again on next entry
                if (_triggered) {
                    _triggered = false;
                    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "AUTO", "Automation reset due to empty room"));
                }
            }
        }
    }

    // Trigger logic
    if (currentLightPercentage <= _threshold) {
        if (!_triggered) {
            // Only trigger if we don't know the room is empty
            if (!roomEmpty) {
                _triggered = true;
                sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "AUTO", String("Light Automation Triggered! Light: ") + currentLightPercentage + " <= " + _threshold));
                
                for (uint8_t pin : _loadPins) {
                    Relay* relay = room.findRelayByPin(pin);
                    if (relay != nullptr) {
                        relay->turnOn();
                        sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "AUTO", String("Turned ON load pin ") + pin));
                    }
                }
            }
        }
    } 
    // Hysteresis reset logic
    else if (currentLightPercentage > (_threshold + 1.0f)) {
        // Only reset based on high light level if the loads are actually OFF.
        // If the loads are ON, the high light might be artificial, so we shouldn't reset.
        bool anyLoadOn = false;
        for (uint8_t pin : _loadPins) {
            Relay* relay = room.findRelayByPin(pin);
            if (relay != nullptr && relay->isOn()) {
                anyLoadOn = true;
                break;
            }
        }

        if (!anyLoadOn && _triggered) {
            _triggered = false;
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "AUTO", String("Light Automation Reset. Natural light high: ") + currentLightPercentage));
        }
    }
}

inline void ClimateAutomation::evaluate(float currentTemp, Room& room) {
    if (!_enabled) {
        _triggered = false;
        return;
    }

    if (isnan(currentTemp)) return;

    if (currentTemp >= _targetTemp) {
        if (!_triggered) {
            _triggered = true;
            if (_irSlotPowerOn >= 0) {
                IRController* ir = room.getIRController();
                if (ir != nullptr) {
                    ir->transmitSlot(_irSlotPowerOn);
                    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "AUTO", String("Climate Auto: Temp ") + currentTemp + " >= " + _targetTemp + ". Emitted ON command."));
                }
            }
        }
    } else if (currentTemp <= (_targetTemp - 1.0f)) { // 1 degree hysteresis
        if (_triggered) {
            _triggered = false;
            if (_irSlotPowerOff >= 0) {
                IRController* ir = room.getIRController();
                if (ir != nullptr) {
                    ir->transmitSlot(_irSlotPowerOff);
                    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "AUTO", String("Climate Auto: Temp ") + currentTemp + " <= " + (_targetTemp - 1.0f) + ". Emitted OFF command."));
                }
            }
        }
    }
}

inline bool SceneManager::executeScene(uint8_t id, Room& room) {
    for (size_t i = 0; i < _scenes.size(); i++) {
        if (_scenes[i].id == id) {
            sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "SCENE", String("Executing scene: ") + _scenes[i].name));
            for (const auto& action : _scenes[i].actions) {
                Relay* relay = room.findRelayByPin(action.pin);
                if (relay) {
                    if (action.state) relay->turnOn();
                    else relay->turnOff();
                }
            }
            return true;
        }
    }
    return false;
}

#endif