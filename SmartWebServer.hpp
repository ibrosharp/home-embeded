#ifndef SMART_WEBSERVER_H
#define SMART_WEBSERVER_H

#include <WebServer.h>
#include "StorageManager.hpp"
#include "Room.hpp"
#include "TaskQueueManager.hpp"
#include "IRTransmitTask.hpp"
#include "IRListenerTask.hpp"

extern TaskQueueManager sysQueue;
extern IRCaptureRequest irCaptureRequest;

class SmartWebServer {
public:
    enum ServerMode {
        MODE_OFFLINE,
        MODE_CONFIG_AP,  // Access Point Mode (Show Wi-Fi Setup Page)
        MODE_OPERATIONAL // Station Mode (Show Smart Home Control Dashboard)
    };

private:
    WebServer _server;
    ServerMode _currentMode;
    StorageManager& _storage;

    // Internal HTTP Endpoint Handlers
    void handleRootOperational();
    void handleRootConfig();
    void handleNotFound();
    void handleSaveConfig();
    void handleGetIRDatabaseAPI();
    void handleRecordIRCommand(); // POST /api/ir/record
    void handleEmitIRCommand();   // POST /api/ir/emit
    void handleGetRoom();
    void handleUpdateSettings();
    void handleSwitchState();   // POST /api/switch/state
    void handleSwitchRelay();   // POST /api/switch/relay

public:
    SmartWebServer(StorageManager& storage, uint16_t port = 80);
    
    // Configures endpoints based on the current network state
    void begin(ServerMode mode);
    
    // Must be called inside your FreeRTOS network loop
    void handleClient();
};

// Implementation

SmartWebServer::SmartWebServer(StorageManager& storage, uint16_t port) 
    : _server(port), _currentMode(MODE_OFFLINE), _storage(storage) {}

void SmartWebServer::begin(ServerMode mode) {
    _currentMode = mode;
    _server.stop(); 
    _server.on("/api/config", [this]() { this->handleRootConfig(); });
    _server.on("/api/save", HTTP_POST, [this]() { this->handleSaveConfig(); });
    _server.on("/api/status", [this]() { this->handleRootOperational(); });
    _server.on("/api/room", [this]() { this->handleGetRoom(); });
    _server.on("/api/ir/commands", [this]() { this->handleGetIRDatabaseAPI(); });
    _server.on("/api/ir/record", [this]() { this->handleRecordIRCommand(); });
    _server.on("/api/ir/emit", [this]() { this->handleEmitIRCommand(); });
    _server.on("/api/settings", [this]() { this->handleUpdateSettings(); });
    _server.on("/api/switch/state", [this]() { this->handleSwitchState(); });
    _server.on("/api/switch/relay", [this]() { this->handleSwitchRelay(); });
    _server.onNotFound([this]() { this->handleNotFound(); });
    _server.begin();
}

void SmartWebServer::handleClient() {
    if (_currentMode != MODE_OFFLINE) {
        _server.handleClient();
    }
}

// GET /api/config (When in AP mode)
void SmartWebServer::handleRootConfig() {
    String json = "{\"status\":\"provisioning\",\"message\":\"Device is ready for Wi-Fi configuration.\"}";
    _server.send(200, "application/json", json);
}

// GET /api/status (When in Operational mode)
void SmartWebServer::handleRootOperational() {
    String json = "{\"status\":\"operational\",\"system_stable\":true,\"message\":\"Task-Driven Home Node online.\"}";
    _server.send(200, "application/json", json);
}

// POST /api/save
void SmartWebServer::handleSaveConfig() {
    if (_server.method() != HTTP_POST) {
        _server.send(405, "application/json", "{\"success\":false,\"error\":\"Method not allowed\"}");
        return;
    }

    String ssid = _server.arg("ssid");
    String pass = _server.arg("pass");

    String json;
    if (ssid.length() > 0) {
        Room& room = Room::getInstance();
        Setting* settings = room.getSetting();
        if (settings != nullptr) {
            settings->setWifi(ssid, pass);
            settings->save(_storage.prefs());
        } else {
            _storage.saveWifiCredentials(ssid, pass);
        }
        Serial.println("[STORAGE] Successfully committed new config to Flash.");

        json = "{\"success\":true,\"message\":\"Credentials received. Rebooting to connect...\"}";
        _server.send(200, "application/json", json);
        
        vTaskDelay(pdMS_TO_TICKS(500)); 
        ESP.restart(); 
    } else {
        json = "{\"success\":false,\"error\":\"Missing required parameter 'ssid'\"}";
        _server.send(400, "application/json", json);
    }
}

// 404 Handler
void SmartWebServer::handleNotFound() {
    String json = "{\"error\":\"Resource not found\",\"code\":404}";
    _server.send(404, "application/json", json);
}

// GET /api/ir/commands (Returns the recorded IR database)
void SmartWebServer::handleGetIRDatabaseAPI() {
    Room& room = Room::getInstance();
    IRController* ir = room.getIRController();
    if (ir == nullptr) {
        _server.send(404, "application/json", "{\"success\":false,\"error\":\"IR controller not available\"}");
        return;
    }

    _server.send(200, "application/json", ir->toJson());
}

// POST /api/ir/record
// Required params: slot, deviceId, name
void SmartWebServer::handleRecordIRCommand() {
    if (_server.method() != HTTP_POST) {
        _server.send(405, "application/json", "{\"success\":false,\"error\":\"Method not allowed\"}");
        return;
    }

    if (!_server.hasArg("slot") || !_server.hasArg("deviceId") || !_server.hasArg("name")) {
        _server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing required params: slot, deviceId, name\"}");
        return;
    }

    int slot = _server.arg("slot").toInt();
    String deviceId = _server.arg("deviceId");
    String name = _server.arg("name");

    if (slot < 0 || slot >= MAX_IR_COMMANDS) {
        _server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid slot index\"}");
        return;
    }

    if (deviceId.length() == 0 || name.length() == 0) {
        _server.send(400, "application/json", "{\"success\":false,\"error\":\"deviceId and name cannot be empty\"}");
        return;
    }

    // Submit the capture request for the low-priority IR listener task.
    irCaptureRequest.slot = slot;
    irCaptureRequest.pending = true;
    strncpy(irCaptureRequest.deviceId, deviceId.c_str(), sizeof(irCaptureRequest.deviceId) - 1);
    strncpy(irCaptureRequest.name, name.c_str(), sizeof(irCaptureRequest.name) - 1);
    irCaptureRequest.deviceId[sizeof(irCaptureRequest.deviceId) - 1] = '\0';
    irCaptureRequest.name[sizeof(irCaptureRequest.name) - 1] = '\0';

    _server.send(200, "application/json", "{\"success\":true,\"message\":\"IR record request queued\"}");
}

// POST /api/ir/emit
// Required params: slot
void SmartWebServer::handleEmitIRCommand() {
    if (_server.method() != HTTP_POST) {
        _server.send(405, "application/json", "{\"success\":false,\"error\":\"Method not allowed\"}");
        return;
    }
    if (!_server.hasArg("slot")) {
        _server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing required param: slot\"}");
        return;
    }

    int slot = _server.arg("slot").toInt();
    if (slot < 0 || slot >= MAX_IR_COMMANDS) {
        _server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid slot index\"}");
        return;
    }

    Room& room = Room::getInstance();
    IRController* ir = room.getIRController();
    if (ir == nullptr) {
        _server.send(404, "application/json", "{\"success\":false,\"error\":\"IR controller not available\"}");
        return;
    }

    IRCommand cmd = ir->getCommand(slot);
    if (!cmd.isValid) {
        _server.send(404, "application/json", "{\"success\":false,\"error\":\"No IR command recorded in requested slot\"}");
        return;
    }

    bool queued = sysQueue.push(new IRTransmitTask(*ir, slot));
    if (!queued) {
        _server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to queue IR transmit task\"}");
        return;
    }

    _server.send(200, "application/json", "{\"success\":true,\"message\":\"IR emit request queued\"}");
}

// GET /api/room (Returns full room state as JSON)
void SmartWebServer::handleGetRoom() {
    Room& room = Room::getInstance();
    String roomJson = room.toJson();
    _server.send(200, "application/json", roomJson);
}

// GET or POST /api/settings
void SmartWebServer::handleUpdateSettings() {
    Room& room = Room::getInstance();
    Setting* settings = room.getSetting();
    if (settings == nullptr) {
        _server.send(404, "application/json", "{\"success\":false,\"error\":\"Settings not registered in Room\"}");
        return;
    }

    if (_server.method() == HTTP_GET) {
        _server.send(200, "application/json", settings->toJson());
        return;
    }

    if (_server.method() == HTTP_POST) {
        bool updated = false;
        if (_server.hasArg("ssid") || _server.hasArg("wifiSSID")) {
            String ssid = _server.hasArg("ssid") ? _server.arg("ssid") : _server.arg("wifiSSID");
            String pass = _server.hasArg("pass") ? _server.arg("pass") : _server.arg("wifiPassword");
            if (!_server.hasArg("pass") && !_server.hasArg("wifiPassword")) {
                pass = settings->getWifiPassword();
            }
            settings->setWifi(ssid, pass);
            updated = true;
        }
        if (_server.hasArg("temp") || _server.hasArg("defaultTargetTemp")) {
            float temp = (_server.hasArg("temp") ? _server.arg("temp") : _server.arg("defaultTargetTemp")).toFloat();
            settings->setTargetTemp(temp);
            updated = true;
        }
        if (_server.hasArg("led") || _server.hasArg("ledFeedbackEnabled")) {
            String ledVal = _server.hasArg("led") ? _server.arg("led") : _server.arg("ledFeedbackEnabled");
            bool ledEnabled = (ledVal == "true" || ledVal == "1");
            settings->setLedFeedbackEnabled(ledEnabled);
            updated = true;
        }

        if (updated) {
            if (settings->save(_storage.prefs())) {
                _server.send(200, "application/json", "{\"success\":true,\"message\":\"Settings updated and saved successfully\",\"settings\":" + settings->toJson() + "}");
            } else {
                _server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to save settings to flash storage\"}");
            }
        } else {
            _server.send(400, "application/json", "{\"success\":false,\"error\":\"No valid setting parameters provided\"}");
        }
        return;
    }

    _server.send(405, "application/json", "{\"success\":false,\"error\":\"Method not allowed\"}");
}

// POST /api/switch/state
// Body params: pin (required)
void SmartWebServer::handleSwitchState() {
    if (_server.method() != HTTP_POST) {
        _server.send(405, "application/json", "{\"success\":false,\"error\":\"Method not allowed\"}");
        return;
    }
    if (!_server.hasArg("pin")) {
        _server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing required param: 'pin'\"}");
        return;
    }
    uint8_t pin = (uint8_t)_server.arg("pin").toInt();

    Room& room = Room::getInstance();
    Switch* sw = room.findSwitchByPin(pin);
    if (sw == nullptr) {
        _server.send(404, "application/json", "{\"success\":false,\"error\":\"Switch not found for given pin\"}");
        return;
    }

    uint8_t newState = (sw->getState() == HIGH) ? LOW : HIGH;
    sw->setState(newState);
    _server.send(200, "application/json", "{\"success\":true,\"message\":\"Switch toggled\",\"switch\":" + sw->toJson() + "}");
}

// POST /api/switch/relay
// Body params: pin (required, switch pin)
//              relayPin (optional — omit or send -1 to detach)
void SmartWebServer::handleSwitchRelay() {
    if (_server.method() != HTTP_POST) {
        _server.send(405, "application/json", "{\"success\":false,\"error\":\"Method not allowed\"}");
        return;
    }
    if (!_server.hasArg("pin")) {
        _server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing required param: 'pin'\"}");
        return;
    }
    uint8_t switchPin = (uint8_t)_server.arg("pin").toInt();

    Room& room = Room::getInstance();
    Switch* sw = room.findSwitchByPin(switchPin);
    if (sw == nullptr) {
        _server.send(404, "application/json", "{\"success\":false,\"error\":\"Switch not found for given pin\"}");
        return;
    }

    // relayPin absent or -1 means detach
    if (!_server.hasArg("relayPin") || _server.arg("relayPin").toInt() < 0) {
        sw->attachRelay(nullptr); // detach
        _server.send(200, "application/json", "{\"success\":true,\"message\":\"Relay detached from switch\",\"switch\":" + sw->toJson() + "}");
        return;
    }

    uint8_t relayPin = (uint8_t)_server.arg("relayPin").toInt();
    Relay* relay = room.findRelayByPin(relayPin);
    if (relay == nullptr) {
        _server.send(404, "application/json", "{\"success\":false,\"error\":\"Relay not found for given relayPin\"}");
        return;
    }
    sw->attachRelay(relay);
    _server.send(200, "application/json", "{\"success\":true,\"message\":\"Relay attached to switch\",\"switch\":" + sw->toJson() + "}");
}

#endif
