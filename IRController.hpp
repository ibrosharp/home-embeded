#ifndef IR_CONTROLLER_H
#define IR_CONTROLLER_H

#include <Arduino.h>
#include <IRremote.h>
#include "JsonSerializable.hpp"

const int MAX_IR_COMMANDS = 60;

struct IRCommand {
    decode_type_t protocol = UNKNOWN;
    uint16_t address = 0;
    uint16_t command = 0;
    uint8_t numberOfBits = 0;
    
    // 🎯 New tracking fields
    String deviceId = ""; // e.g., "tv_lg" or "ac_panasonic"
    String name = "";     // e.g., "Volume Up", "Power Off"
    
    bool isValid = false;
};

#include "PreferenceModel.hpp"
#include "StorageManager.hpp"

class IRController : public PreferenceModel, public JsonSerializable {
private:
    uint8_t _recvPin;
    uint8_t _sendPin;
    IRCommand _commandDatabase[MAX_IR_COMMANDS];
    StorageManager* _storage;

public:
    IRController(uint8_t recvPin, uint8_t sendPin, StorageManager* storage = nullptr);
    
    void init();
    
    // ---- Recording & Database Management ----
    void saveCustomCommand(int slotIndex, const IRCommand& cmd);
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

void IRController::saveCustomCommand(int slotIndex, const IRCommand& cmd) {
    if (slotIndex >= 0 && slotIndex < MAX_IR_COMMANDS) {
        _commandDatabase[slotIndex] = cmd;
        if (_storage != nullptr) {
            save(_storage->prefs());
        }
    }
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
    json += "\"commands\":[";
    
    bool firstCommand = true;
    for (int i = 0; i < MAX_IR_COMMANDS; i++) {
        if (_commandDatabase[i].isValid) {
            if (!firstCommand) json += ",";
            json += "{";
            json += "\"slot\":" + String(i) + ",";
            json += "\"deviceId\":\"" + _commandDatabase[i].deviceId + "\",";
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

    Serial.printf("[IR] Blasting '%s' for Device '%s'\n", cmd.name.c_str(), cmd.deviceId.c_str());
    
    // 🎯 Fixes: no matching function for call to 'IRsend::write(const uint16_t*, const uint8_t&)'
    // Invoke explicit protocol arguments layout: protocol, address, command, numberOfRepeats
    IrSender.write(cmd.protocol, cmd.address, cmd.command, 0);

    // 🎯 Fixes: no matching function for call to 'IRrecv::start(uint8_t&)'
    IrReceiver.start(); 
}

bool IRController::save(Preferences &prefs) {
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
            
            snprintf(key, sizeof(key), "ird_%d", i);
            prefs.putString(key, _commandDatabase[i].deviceId);
            
            snprintf(key, sizeof(key), "irn_%d", i);
            prefs.putString(key, _commandDatabase[i].name);
        }
    }
    return true;
}

bool IRController::load(Preferences &prefs) {
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
            
            snprintf(key, sizeof(key), "ird_%d", i);
            _commandDatabase[i].deviceId = prefs.getString(key, "");
            
            snprintf(key, sizeof(key), "irn_%d", i);
            _commandDatabase[i].name = prefs.getString(key, "");
        } else {
            _commandDatabase[i] = IRCommand();
        }
    }
    return true;
}

#endif
