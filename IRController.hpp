#ifndef IR_CONTROLLER_H
#define IR_CONTROLLER_H

#include <Arduino.h>
#include <IRremote.h>
#include "JsonSerializable.hpp"

const int MAX_IR_DEVICES = 4;
const int COMMANDS_PER_DEVICE = 15;
const int MAX_IR_COMMANDS = MAX_IR_DEVICES * COMMANDS_PER_DEVICE; // 60

struct IRDevice {
    char name[32] = "";
    bool isValid = false;
};

struct IRCommand {
    decode_type_t protocol = UNKNOWN;
    uint16_t address = 0;
    uint16_t command = 0;
    uint8_t numberOfBits = 0;
    String name = "";     // e.g., "Volume Up", "Power Off"
    bool isValid = false;
};

#include "PreferenceModel.hpp"
#include "StorageManager.hpp"

class IRController : public PreferenceModel, public JsonSerializable {
private:
    uint8_t _recvPin;
    uint8_t _sendPin;
    IRDevice _devices[MAX_IR_DEVICES];
    IRCommand _commandDatabase[MAX_IR_COMMANDS];
    StorageManager* _storage;

public:
    IRController(uint8_t recvPin, uint8_t sendPin, StorageManager* storage = nullptr);
    
    void init();
    
    // ---- Device Management ----
    int createDevice(const String& name);
    bool deleteDevice(int deviceId);
    bool isDeviceValid(int deviceId) const;

    // ---- Recording & Database Management ----
    bool saveCustomCommand(int deviceId, int buttonIndex, const IRCommand& cmd);
    IRCommand getCommand(int slotIndex) const;

    // ---- Transmission ----
    bool transmitSlot(int slotIndex);
    void transmitRaw(const IRCommand& cmd);
    String toJson() override;

    bool save(Preferences &prefs) override;
    bool load(Preferences &prefs) override;
};

// Implementation

IRController::IRController(uint8_t recvPin, uint8_t sendPin, StorageManager* storage) 
    : _recvPin(recvPin), _sendPin(sendPin), _storage(storage) {
    for (int i = 0; i < MAX_IR_DEVICES; i++) {
        _devices[i] = IRDevice();
    }
    for (int i = 0; i < MAX_IR_COMMANDS; i++) {
        _commandDatabase[i] = IRCommand();
    }
}

void IRController::init() {
    pinMode(_recvPin, INPUT);
    pinMode(_sendPin, OUTPUT);
    IrReceiver.begin(_recvPin, DISABLE_LED_FEEDBACK);
    IrSender.begin(_sendPin, DISABLE_LED_FEEDBACK);
}

int IRController::createDevice(const String& name) {
    for (int i = 0; i < MAX_IR_DEVICES; i++) {
        if (!_devices[i].isValid) {
            _devices[i].isValid = true;
            strncpy(_devices[i].name, name.c_str(), sizeof(_devices[i].name) - 1);
            _devices[i].name[sizeof(_devices[i].name) - 1] = '\0';
            
            // Clear its reserved slots just in case
            int startSlot = i * COMMANDS_PER_DEVICE;
            for (int j = 0; j < COMMANDS_PER_DEVICE; j++) {
                _commandDatabase[startSlot + j] = IRCommand();
            }

            if (_storage != nullptr) {
                save(_storage->prefs());
            }
            return i;
        }
    }
    return -1; // No free devices
}

bool IRController::deleteDevice(int deviceId) {
    if (deviceId >= 0 && deviceId < MAX_IR_DEVICES) {
        _devices[deviceId].isValid = false;
        
        // Clear slots
        int startSlot = deviceId * COMMANDS_PER_DEVICE;
        for (int j = 0; j < COMMANDS_PER_DEVICE; j++) {
            _commandDatabase[startSlot + j] = IRCommand();
        }

        if (_storage != nullptr) {
            save(_storage->prefs());
        }
        return true;
    }
    return false;
}

bool IRController::isDeviceValid(int deviceId) const {
    if (deviceId >= 0 && deviceId < MAX_IR_DEVICES) {
        return _devices[deviceId].isValid;
    }
    return false;
}

bool IRController::saveCustomCommand(int deviceId, int buttonIndex, const IRCommand& cmd) {
    if (deviceId >= 0 && deviceId < MAX_IR_DEVICES && buttonIndex >= 0 && buttonIndex < COMMANDS_PER_DEVICE) {
        if (!_devices[deviceId].isValid) return false;
        
        int slotIndex = deviceId * COMMANDS_PER_DEVICE + buttonIndex;
        _commandDatabase[slotIndex] = cmd;
        if (_storage != nullptr) {
            save(_storage->prefs());
        }
        return true;
    }
    return false;
}

IRCommand IRController::getCommand(int slotIndex) const {
    if (slotIndex >= 0 && slotIndex < MAX_IR_COMMANDS) {
        return _commandDatabase[slotIndex];
    }
    return IRCommand();
}

String IRController::toJson() {
    String json = "{";
    json += "\"type\":\"irController\",";
    json += "\"recvPin\":" + String(_recvPin) + ",";
    json += "\"sendPin\":" + String(_sendPin) + ",";
    
    // Devices
    json += "\"devices\":[";
    bool firstDevice = true;
    for (int i = 0; i < MAX_IR_DEVICES; i++) {
        if (_devices[i].isValid) {
            if (!firstDevice) json += ",";
            json += "{";
            json += "\"id\":" + String(i) + ",";
            json += "\"name\":\"" + String(_devices[i].name) + "\"";
            json += "}";
            firstDevice = false;
        }
    }
    json += "],";

    // Commands
    json += "\"commands\":[";
    bool firstCommand = true;
    for (int i = 0; i < MAX_IR_COMMANDS; i++) {
        if (_commandDatabase[i].isValid) {
            if (!firstCommand) json += ",";
            int deviceId = i / COMMANDS_PER_DEVICE;
            int buttonIndex = i % COMMANDS_PER_DEVICE;

            json += "{";
            json += "\"slot\":" + String(i) + ",";
            json += "\"deviceId\":" + String(deviceId) + ",";
            json += "\"buttonIndex\":" + String(buttonIndex) + ",";
            json += "\"name\":\"" + _commandDatabase[i].name + "\",";
            json += "\"protocol\":" + String(_commandDatabase[i].protocol) + ",";
            json += "\"address\":\"0x" + String(_commandDatabase[i].address, HEX) + "\",";
            json += "\"command\":\"0x" + String(_commandDatabase[i].command, HEX) + "\"";
            json += "}";
            firstCommand = false;
        }
    }
    json += "]";
    json += "}";
    return json;
}

bool IRController::transmitSlot(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= MAX_IR_COMMANDS) return false;
    
    // Check if the device is actually valid
    int deviceId = slotIndex / COMMANDS_PER_DEVICE;
    if (!_devices[deviceId].isValid) {
        Serial.printf("[IR] Cannot transmit slot %d: Device %d is not valid.\n", slotIndex, deviceId);
        return false;
    }

    const IRCommand& cmd = _commandDatabase[slotIndex];
    if (!cmd.isValid) {
        Serial.printf("[IR] Cannot transmit slot %d: Slot is empty/unmapped.\n", slotIndex);
        return false;
    }

    transmitRaw(cmd);
    return true;
}

void IRController::transmitRaw(const IRCommand& cmd) {
    IrReceiver.stop();

    Serial.printf("[IR] Blasting '%s'\n", cmd.name.c_str());
    
    // Invoke explicit protocol arguments layout: protocol, address, command, numberOfRepeats
    IrSender.write(cmd.protocol, cmd.address, cmd.command, 0);

    IrReceiver.start(); 
}

bool IRController::save(Preferences &prefs) {
    // Save Devices
    for (int i = 0; i < MAX_IR_DEVICES; i++) {
        char key[16];
        snprintf(key, sizeof(key), "ir_dv_%d", i);
        prefs.putBool(key, _devices[i].isValid);
        
        if (_devices[i].isValid) {
            snprintf(key, sizeof(key), "ir_dn_%d", i);
            prefs.putString(key, String(_devices[i].name));
        }
    }

    // Save Commands
    for (int i = 0; i < MAX_IR_COMMANDS; i++) {
        char key[16];
        
        snprintf(key, sizeof(key), "irv_%d", i);
        prefs.putBool(key, _commandDatabase[i].isValid);
        
        if (_commandDatabase[i].isValid) {
            snprintf(key, sizeof(key), "irp_%d", i);
            prefs.putUChar(key, (uint8_t)_commandDatabase[i].protocol);
            
            snprintf(key, sizeof(key), "ira_%d", i);
            prefs.putUShort(key, _commandDatabase[i].address);
            
            snprintf(key, sizeof(key), "irc_%d", i);
            prefs.putUShort(key, _commandDatabase[i].command);
            
            snprintf(key, sizeof(key), "irb_%d", i);
            prefs.putUChar(key, _commandDatabase[i].numberOfBits);
            
            snprintf(key, sizeof(key), "irn_%d", i);
            prefs.putString(key, _commandDatabase[i].name);
        }
    }
    return true;
}

bool IRController::load(Preferences &prefs) {
    // Load Devices
    for (int i = 0; i < MAX_IR_DEVICES; i++) {
        char key[16];
        snprintf(key, sizeof(key), "ir_dv_%d", i);
        _devices[i].isValid = prefs.getBool(key, false);
        
        if (_devices[i].isValid) {
            snprintf(key, sizeof(key), "ir_dn_%d", i);
            String name = prefs.getString(key, "");
            strncpy(_devices[i].name, name.c_str(), sizeof(_devices[i].name) - 1);
            _devices[i].name[sizeof(_devices[i].name) - 1] = '\0';
        } else {
            _devices[i] = IRDevice();
        }
    }

    // Load Commands
    for (int i = 0; i < MAX_IR_COMMANDS; i++) {
        char key[16];
        snprintf(key, sizeof(key), "irv_%d", i);
        _commandDatabase[i].isValid = prefs.getBool(key, false);
        
        if (_commandDatabase[i].isValid) {
            snprintf(key, sizeof(key), "irp_%d", i);
            _commandDatabase[i].protocol = (decode_type_t)prefs.getUChar(key, UNKNOWN);
            
            snprintf(key, sizeof(key), "ira_%d", i);
            _commandDatabase[i].address = prefs.getUShort(key, 0);
            
            snprintf(key, sizeof(key), "irc_%d", i);
            _commandDatabase[i].command = prefs.getUShort(key, 0);
            
            snprintf(key, sizeof(key), "irb_%d", i);
            _commandDatabase[i].numberOfBits = prefs.getUChar(key, 0);
            
            snprintf(key, sizeof(key), "irn_%d", i);
            _commandDatabase[i].name = prefs.getString(key, "");
        } else {
            _commandDatabase[i] = IRCommand();
        }
    }
    return true;
}

#endif
