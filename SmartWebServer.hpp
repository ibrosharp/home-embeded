#ifndef SMART_WEB_SERVER_H
#define SMART_WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "StorageManager.hpp"
#include "Room.hpp"
#include "TaskQueueManager.hpp"
#include "IRTransmitTask.hpp"
#include "IRListenerTask.hpp"
#include "BootstrapFS.hpp"
#include <Update.h>

extern TaskQueueManager sysQueue;
extern IRCaptureRequest irCaptureRequest;

class SmartWebServer {
public:
    enum ServerMode {
        MODE_OFFLINE,
        MODE_CONFIG_AP,
        MODE_OPERATIONAL
    };

private:
    AsyncWebServer _server;
    ServerMode _currentMode;
    StorageManager& _storage;

    bool isAuthenticated(AsyncWebServerRequest *request) {
        Setting* setting = Room::getInstance().getSetting();
        if (setting) {
            return request->authenticate(setting->getAuthUsername().c_str(), setting->getAuthPassword().c_str());
        }
        return request->authenticate("admin", "admin");
    }

    String getArg(AsyncWebServerRequest *request, const String& name) {
        if (request->hasParam(name, true)) return request->getParam(name, true)->value();
        if (request->hasParam(name, false)) return request->getParam(name, false)->value();
        return "";
    }

    bool hasArg(AsyncWebServerRequest *request, const String& name) {
        return request->hasParam(name, true) || request->hasParam(name, false);
    }

    void handleRootConfig(AsyncWebServerRequest *request);
    void handleRootOperational(AsyncWebServerRequest *request);
    void handleNotFound(AsyncWebServerRequest *request);
    void handleSaveConfig(AsyncWebServerRequest *request);
    void handleGetIRDatabaseAPI(AsyncWebServerRequest *request);
    void handleRecordIRCommand(AsyncWebServerRequest *request); 
    void handleEmitIRCommand(AsyncWebServerRequest *request);   
    void handleGetRoom(AsyncWebServerRequest *request);
    void handleUpdateSettings(AsyncWebServerRequest *request);
    void handleSwitchState(AsyncWebServerRequest *request);   
    void handleSwitchLoad(AsyncWebServerRequest *request);   
    void handleLoadToggle(AsyncWebServerRequest *request);
    
    // Automation
    void handleLightAutomation(AsyncWebServerRequest *request);
    void handleUpdateLightAutomation(AsyncWebServerRequest *request);
    void handleClimateAutomation(AsyncWebServerRequest *request);
    void handleUpdateClimateAutomation(AsyncWebServerRequest *request);
    
    // Scenes
    void handleCreateScene(AsyncWebServerRequest *request);
    void handleDeleteScene(AsyncWebServerRequest *request);
    void handleExecuteScene(AsyncWebServerRequest *request);
    
    // History
    void handleHistory(AsyncWebServerRequest *request);
    
    // OTA Endpoints
    void handleUpdateGet(AsyncWebServerRequest *request);
    void handleUpdatePost(AsyncWebServerRequest *request);
    void handleUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

public:
    SmartWebServer(StorageManager& storage, uint16_t port = 80);
    
    // Configures endpoints based on the current network state
    void begin(ServerMode mode);
    
    // Must be called inside your FreeRTOS network loop (No-op for async, kept for compatibility)
    void handleClient();
};

// Implementation

SmartWebServer::SmartWebServer(StorageManager& storage, uint16_t port) 
    : _server(port), _currentMode(MODE_OFFLINE), _storage(storage) {}

void SmartWebServer::begin(ServerMode mode) {
    _currentMode = mode;
    
    if (!LittleFS.begin(true)) {
        Serial.println("An Error has occurred while mounting LittleFS");
    } else {
        bootstrapLittleFS();
    }

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "content-type");

    #define REQUIRE_AUTH if (!this->isAuthenticated(request)) return request->requestAuthentication("Login Required", false)

    auto serveFile = [this](AsyncWebServerRequest *request, const char* path, const char* mime) {
        REQUIRE_AUTH;
        request->send(LittleFS, path, mime);
    };

    _server.on("/", HTTP_GET, [this, serveFile](AsyncWebServerRequest *request) { serveFile(request, "/index.html", "text/html"); });
    _server.on("/style.css", HTTP_GET, [this, serveFile](AsyncWebServerRequest *request) { serveFile(request, "/style.css", "text/css"); });
    _server.on("/app.js", HTTP_GET, [this, serveFile](AsyncWebServerRequest *request) { serveFile(request, "/app.js", "application/javascript"); });

    _server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleRootConfig(request); });
    _server.on("/api/save", HTTP_POST, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleSaveConfig(request); });
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleRootOperational(request); });
    _server.on("/api/room", HTTP_GET, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleGetRoom(request); });
    _server.on("/api/ir/commands", HTTP_GET, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleGetIRDatabaseAPI(request); });
    _server.on("/api/ir/record", HTTP_POST, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleRecordIRCommand(request); });
    _server.on("/api/ir/emit", HTTP_POST, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleEmitIRCommand(request); });
    _server.on("/api/settings", HTTP_ANY, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleUpdateSettings(request); });
    _server.on("/api/switch/state", HTTP_POST, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleSwitchState(request); });
    _server.on("/api/switch/load", HTTP_POST, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleSwitchLoad(request); });
    _server.on("/api/load/toggle", HTTP_POST, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleLoadToggle(request); });
    _server.on("/api/automation/light", HTTP_GET, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleLightAutomation(request); });
    _server.on("/api/automation/light", HTTP_POST, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleUpdateLightAutomation(request); });
    _server.on("/api/automation/climate", HTTP_GET, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleClimateAutomation(request); });
    _server.on("/api/automation/climate", HTTP_POST, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleUpdateClimateAutomation(request); });
    _server.on("/api/history", HTTP_GET, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleHistory(request); });
    
    // Scenes
    _server.on("/api/scenes/execute", HTTP_POST, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleExecuteScene(request); });
    _server.on("/api/scenes", HTTP_POST, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleCreateScene(request); });
    _server.on("/api/scenes", HTTP_DELETE, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleDeleteScene(request); });
    
    // OTA Routes
    _server.on("/update", HTTP_GET, [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleUpdateGet(request); });
    _server.on("/update", HTTP_POST, 
        [this](AsyncWebServerRequest *request) { REQUIRE_AUTH; this->handleUpdatePost(request); }, 
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            Setting* setting = Room::getInstance().getSetting();
            if (setting && !request->authenticate(setting->getAuthUsername().c_str(), setting->getAuthPassword().c_str())) return;
            this->handleUpdateUpload(request, filename, index, data, len, final);
        }
    );

    _server.onNotFound([this](AsyncWebServerRequest *request) { 
        if (request->method() == HTTP_OPTIONS) {
            request->send(204);
            return;
        }
        this->handleNotFound(request); 
    });
    _server.begin();
}

void SmartWebServer::handleClient() {
    // ESPAsyncWebServer runs in a background thread. No polling needed.
}

// GET /api/config (When in AP mode)
void SmartWebServer::handleRootConfig(AsyncWebServerRequest *request) {
    String json = "{\"status\":\"provisioning\",\"message\":\"Device is ready for Wi-Fi configuration.\"}";
    request->send(200, "application/json", json);
}

// GET /api/status (When in Operational mode)
void SmartWebServer::handleRootOperational(AsyncWebServerRequest *request) {
    String json = "{\"status\":\"operational\",\"system_stable\":true,\"message\":\"Task-Driven Home Node online.\"}";
    request->send(200, "application/json", json);
}

// POST /api/save
void SmartWebServer::handleSaveConfig(AsyncWebServerRequest *request) {
    String ssid = getArg(request, "ssid");
    String pass = getArg(request, "pass");

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
        request->send(200, "application/json", json);
        
        vTaskDelay(pdMS_TO_TICKS(500)); 
        ESP.restart(); 
    } else {
        json = "{\"success\":false,\"error\":\"Missing required parameter 'ssid'\"}";
        request->send(400, "application/json", json);
    }
}

// 404 Handler
void SmartWebServer::handleNotFound(AsyncWebServerRequest *request) {
    String json = "{\"error\":\"Resource not found\",\"code\":404}";
    request->send(404, "application/json", json);
}

// =================== SCENES ===================
void SmartWebServer::handleCreateScene(AsyncWebServerRequest *request) {
    if (!hasArg(request, "name") || !hasArg(request, "actions")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
        return;
    }
    
    String name = getArg(request, "name");
    String actionsStr = getArg(request, "actions"); // e.g. "12:1,14:0"
    
    std::vector<SceneAction> actions;
    if (actionsStr.length() > 0) {
        int startIndex = 0;
        int commaIndex = actionsStr.indexOf(',');
        while (commaIndex != -1) {
            String pair = actionsStr.substring(startIndex, commaIndex);
            int colonIndex = pair.indexOf(':');
            if (colonIndex != -1) {
                actions.push_back({(uint8_t)pair.substring(0, colonIndex).toInt(), pair.substring(colonIndex + 1).toInt() > 0});
            }
            startIndex = commaIndex + 1;
            commaIndex = actionsStr.indexOf(',', startIndex);
        }
        if (startIndex < actionsStr.length()) {
            String pair = actionsStr.substring(startIndex);
            int colonIndex = pair.indexOf(':');
            if (colonIndex != -1) {
                actions.push_back({(uint8_t)pair.substring(0, colonIndex).toInt(), pair.substring(colonIndex + 1).toInt() > 0});
            }
        }
    }
    
    Room& room = Room::getInstance();
    SceneManager* sm = room.getSceneManager();
    if (sm) {
        if (sm->addScene(name, actions)) {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Scene created\"}");
        } else {
            request->send(500, "application/json", "{\"success\":false,\"error\":\"Could not create scene (limit reached?)\"}");
        }
    } else {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"SceneManager not found\"}");
    }
}

void SmartWebServer::handleDeleteScene(AsyncWebServerRequest *request) {
    if (!hasArg(request, "id")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing id\"}");
        return;
    }
    
    uint8_t id = getArg(request, "id").toInt();
    Room& room = Room::getInstance();
    SceneManager* sm = room.getSceneManager();
    if (sm) {
        if (sm->deleteScene(id)) {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Scene deleted\"}");
        } else {
            request->send(404, "application/json", "{\"success\":false,\"error\":\"Scene not found\"}");
        }
    } else {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"SceneManager not found\"}");
    }
}

void SmartWebServer::handleExecuteScene(AsyncWebServerRequest *request) {
    if (!hasArg(request, "id")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing id\"}");
        return;
    }
    
    uint8_t id = getArg(request, "id").toInt();
    Room& room = Room::getInstance();
    SceneManager* sm = room.getSceneManager();
    if (sm) {
        if (sm->executeScene(id, room)) {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Scene executed\"}");
        } else {
            request->send(404, "application/json", "{\"success\":false,\"error\":\"Scene not found\"}");
        }
    } else {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"SceneManager not found\"}");
    }
}

// GET /api/ir/commands
void SmartWebServer::handleGetIRDatabaseAPI(AsyncWebServerRequest *request) {
    Room& room = Room::getInstance();
    IRController* ir = room.getIRController();
    if (ir == nullptr) {
        request->send(404, "application/json", "{\"success\":false,\"error\":\"IR controller not available\"}");
        return;
    }

    request->send(200, "application/json", ir->toJson());
}

// POST /api/ir/record
void SmartWebServer::handleRecordIRCommand(AsyncWebServerRequest *request) {
    if (!hasArg(request, "slot") || !hasArg(request, "deviceId") || !hasArg(request, "name")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing required params: slot, deviceId, name\"}");
        return;
    }

    int slot = getArg(request, "slot").toInt();
    String deviceId = getArg(request, "deviceId");
    String name = getArg(request, "name");

    if (slot < 0 || slot >= MAX_IR_COMMANDS) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid slot index\"}");
        return;
    }

    if (deviceId.length() == 0 || name.length() == 0) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"deviceId and name cannot be empty\"}");
        return;
    }

    irCaptureRequest.slot = slot;
    irCaptureRequest.pending = true;
    strncpy(irCaptureRequest.deviceId, deviceId.c_str(), sizeof(irCaptureRequest.deviceId) - 1);
    strncpy(irCaptureRequest.name, name.c_str(), sizeof(irCaptureRequest.name) - 1);
    irCaptureRequest.deviceId[sizeof(irCaptureRequest.deviceId) - 1] = '\0';
    irCaptureRequest.name[sizeof(irCaptureRequest.name) - 1] = '\0';

    request->send(200, "application/json", "{\"success\":true,\"message\":\"IR record request queued\"}");
}

// POST /api/ir/emit
void SmartWebServer::handleEmitIRCommand(AsyncWebServerRequest *request) {
    if (!hasArg(request, "slot")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing required param: slot\"}");
        return;
    }

    int slot = getArg(request, "slot").toInt();
    if (slot < 0 || slot >= MAX_IR_COMMANDS) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid slot index\"}");
        return;
    }

    Room& room = Room::getInstance();
    IRController* ir = room.getIRController();
    if (ir == nullptr) {
        request->send(404, "application/json", "{\"success\":false,\"error\":\"IR controller not available\"}");
        return;
    }

    IRCommand cmd = ir->getCommand(slot);
    if (!cmd.isValid) {
        request->send(404, "application/json", "{\"success\":false,\"error\":\"No IR command recorded in requested slot\"}");
        return;
    }

    bool queued = sysQueue.push(new IRTransmitTask(*ir, slot));
    if (!queued) {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to queue IR transmit task\"}");
        return;
    }

    request->send(200, "application/json", "{\"success\":true,\"message\":\"IR emit request queued\"}");
}

// GET /api/room
void SmartWebServer::handleGetRoom(AsyncWebServerRequest *request) {
    Room& room = Room::getInstance();
    String roomJson = room.toJson();
    request->send(200, "application/json", roomJson);
}

// ANY /api/settings
void SmartWebServer::handleUpdateSettings(AsyncWebServerRequest *request) {
    Room& room = Room::getInstance();
    Setting* settings = room.getSetting();
    if (settings == nullptr) {
        request->send(404, "application/json", "{\"success\":false,\"error\":\"Settings not registered in Room\"}");
        return;
    }

    if (request->method() == HTTP_GET) {
        request->send(200, "application/json", settings->toJson());
        return;
    }

    if (request->method() == HTTP_POST) {
        bool updated = false;
        if (hasArg(request, "ssid") || hasArg(request, "wifiSSID")) {
            String ssid = hasArg(request, "ssid") ? getArg(request, "ssid") : getArg(request, "wifiSSID");
            String pass = hasArg(request, "pass") ? getArg(request, "pass") : getArg(request, "wifiPassword");
            if (!hasArg(request, "pass") && !hasArg(request, "wifiPassword")) {
                pass = settings->getWifiPassword();
            }
            settings->setWifi(ssid, pass);
            updated = true;
        }
        if (hasArg(request, "temp") || hasArg(request, "defaultTargetTemp")) {
            float temp = (hasArg(request, "temp") ? getArg(request, "temp") : getArg(request, "defaultTargetTemp")).toFloat();
            settings->setTargetTemp(temp);
            updated = true;
        }
        if (hasArg(request, "led") || hasArg(request, "ledFeedbackEnabled")) {
            String ledVal = hasArg(request, "led") ? getArg(request, "led") : getArg(request, "ledFeedbackEnabled");
            bool ledEnabled = (ledVal == "true" || ledVal == "1");
            settings->setLedFeedbackEnabled(ledEnabled);
            updated = true;
        }
        if (hasArg(request, "authUser") && hasArg(request, "authPass")) {
            settings->setAuth(getArg(request, "authUser"), getArg(request, "authPass"));
            updated = true;
        }

        if (updated) {
            if (settings->save(_storage.prefs())) {
                request->send(200, "application/json", "{\"success\":true,\"message\":\"Settings updated successfully\",\"settings\":" + settings->toJson() + "}");
            } else {
                request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to save settings\"}");
            }
        } else {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"No valid setting parameters provided\"}");
        }
        return;
    }

    request->send(405, "application/json", "{\"success\":false,\"error\":\"Method not allowed\"}");
}

// POST /api/switch/state
void SmartWebServer::handleSwitchState(AsyncWebServerRequest *request) {
    if (!hasArg(request, "pin")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing required param: 'pin'\"}");
        return;
    }
    uint8_t pin = (uint8_t)getArg(request, "pin").toInt();

    Room& room = Room::getInstance();
    Switch* sw = room.findSwitchByPin(pin);
    if (sw == nullptr) {
        request->send(404, "application/json", "{\"success\":false,\"error\":\"Switch not found for given pin\"}");
        return;
    }

    uint8_t newState = (sw->getState() == HIGH) ? LOW : HIGH;
    sw->setState(newState);
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Switch toggled\",\"switch\":" + sw->toJson() + "}");
}

// POST /api/switch/load
void SmartWebServer::handleSwitchLoad(AsyncWebServerRequest *request) {
    if (!hasArg(request, "pin")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing required param: 'pin'\"}");
        return;
    }
    uint8_t switchPin = (uint8_t)getArg(request, "pin").toInt();

    Room& room = Room::getInstance();
    Switch* sw = room.findSwitchByPin(switchPin);
    if (sw == nullptr) {
        request->send(404, "application/json", "{\"success\":false,\"error\":\"Switch not found for given pin\"}");
        return;
    }

    if (!hasArg(request, "loadPin") || getArg(request, "loadPin").toInt() < 0) {
        sw->attachRelay(nullptr); // detach
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Load detached from switch\",\"switch\":" + sw->toJson() + "}");
        return;
    }

    uint8_t loadPin = (uint8_t)getArg(request, "loadPin").toInt();
    Relay* relay = room.findRelayByPin(loadPin);
    if (relay == nullptr) {
        request->send(404, "application/json", "{\"success\":false,\"error\":\"Load not found\"}");
        return;
    }
    sw->attachRelay(relay);
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Load attached to switch\",\"switch\":" + sw->toJson() + "}");
}

// POST /api/load/toggle
void SmartWebServer::handleLoadToggle(AsyncWebServerRequest *request) {
    if (!hasArg(request, "pin")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing required param: 'pin'\"}");
        return;
    }
    uint8_t pin = (uint8_t)getArg(request, "pin").toInt();

    Room& room = Room::getInstance();
    Relay* relay = room.findRelayByPin(pin);
    
    if (relay == nullptr) {
        request->send(404, "application/json", "{\"success\":false,\"error\":\"Load not found for given pin\"}");
        return;
    }

    relay->toggle();
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Load toggled\",\"load\":" + relay->toJson() + "}");
}

// ======================== OTA UPDATE HANDLERS ========================

// GET /update - Fallback minimal HTML form for uploading firmware
void SmartWebServer::handleUpdateGet(AsyncWebServerRequest *request) {
    String html = "<html><body><h1>AURA Firmware Update</h1>";
    html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
    html += "<input type='file' name='update' accept='.bin'>";
    html += "<input type='submit' value='Upload & Flash'>";
    html += "</form></body></html>";
    request->send(200, "text/html", html);
}

// GET /api/automation/light
void SmartWebServer::handleLightAutomation(AsyncWebServerRequest *request) {
    Room& room = Room::getInstance();
    LightAutomation* autoMgr = room.getLightAutomation();
    if (autoMgr) {
        request->send(200, "application/json", autoMgr->toJson());
    } else {
        request->send(404, "application/json", "{\"error\":\"Automation not found\"}");
    }
}

// POST /api/automation/light
void SmartWebServer::handleUpdateLightAutomation(AsyncWebServerRequest *request) {
    Room& room = Room::getInstance();
    LightAutomation* autoMgr = room.getLightAutomation();
    if (!autoMgr) {
        request->send(404, "application/json", "{\"error\":\"Automation not found\"}");
        return;
    }

    bool enabled = false;
    if (hasArg(request, "enabled")) {
        String enStr = getArg(request, "enabled");
        enabled = (enStr == "true" || enStr == "1");
    }

    float threshold = 2.0f;
    if (hasArg(request, "threshold")) {
        threshold = getArg(request, "threshold").toFloat();
    }

    std::vector<uint8_t> loadPins;
    if (hasArg(request, "loadPins")) {
        String pinsStr = getArg(request, "loadPins");
        if (pinsStr.length() > 0) {
            int startIndex = 0;
            int commaIndex = pinsStr.indexOf(',');
            while (commaIndex != -1) {
                loadPins.push_back((uint8_t)pinsStr.substring(startIndex, commaIndex).toInt());
                startIndex = commaIndex + 1;
                commaIndex = pinsStr.indexOf(',', startIndex);
            }
            if (startIndex < pinsStr.length()) {
                loadPins.push_back((uint8_t)pinsStr.substring(startIndex).toInt());
            }
        }
    }

    bool autoOffEnabled = false;
    if (hasArg(request, "autoOffEnabled")) {
        String enStr = getArg(request, "autoOffEnabled");
        autoOffEnabled = (enStr == "true" || enStr == "1");
    }

    uint16_t autoOffTimeoutSeconds = 300;
    if (hasArg(request, "autoOffTimeoutSeconds")) {
        autoOffTimeoutSeconds = getArg(request, "autoOffTimeoutSeconds").toInt();
    }

    autoMgr->setConfig(enabled, threshold, loadPins, autoOffEnabled, autoOffTimeoutSeconds);
    request->send(200, "application/json", "{\"success\":true}");
}

// POST /update - Final response after the upload stream finishes
void SmartWebServer::handleUpdatePost(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", Update.hasError() ? "{\"success\":false,\"error\":\"Update failed\"}" : "{\"success\":true,\"message\":\"Update success. Rebooting...\"}");
    response->addHeader("Connection", "close");
    request->send(response);
    if (!Update.hasError()) {
        delay(1000);
        ESP.restart();
    }
}

// POST /update - File upload stream handler
void SmartWebServer::handleUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        Serial.printf("[OTA] Update Start: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { 
            Update.printError(Serial);
        }
    }
    if (Update.write(data, len) != len) {
        Update.printError(Serial);
    }
    if (final) {
        if (Update.end(true)) { 
            Serial.printf("[OTA] Update Success: %u B\n", index + len);
        } else {
            Update.printError(Serial);
        }
    }
}

// GET /api/history
void SmartWebServer::handleHistory(AsyncWebServerRequest *request) {
    Room& room = Room::getInstance();
    HistoryBuffer* history = room.getHistoryBuffer();
    if (!history) {
        request->send(404, "application/json", "{\"error\":\"History buffer not found\"}");
        return;
    }
    request->send(200, "application/json", history->toJson());
}

// GET /api/automation/climate
void SmartWebServer::handleClimateAutomation(AsyncWebServerRequest *request) {
    Room& room = Room::getInstance();
    ClimateAutomation* autoMgr = room.getClimateAutomation();
    if (!autoMgr) {
        request->send(404, "application/json", "{\"error\":\"Automation not found\"}");
        return;
    }
    request->send(200, "application/json", autoMgr->toJson());
}

// POST /api/automation/climate
void SmartWebServer::handleUpdateClimateAutomation(AsyncWebServerRequest *request) {
    Room& room = Room::getInstance();
    ClimateAutomation* autoMgr = room.getClimateAutomation();
    if (!autoMgr) {
        request->send(404, "application/json", "{\"error\":\"Automation not found\"}");
        return;
    }

    bool enabled = false;
    if (hasArg(request, "enabled")) {
        String enStr = getArg(request, "enabled");
        enabled = (enStr == "true" || enStr == "1");
    }

    float targetTemp = 24.0f;
    if (hasArg(request, "targetTemp")) {
        targetTemp = getArg(request, "targetTemp").toFloat();
    }

    int irSlotPowerOn = -1;
    if (hasArg(request, "irSlotPowerOn")) {
        irSlotPowerOn = getArg(request, "irSlotPowerOn").toInt();
    }

    int irSlotPowerOff = -1;
    if (hasArg(request, "irSlotPowerOff")) {
        irSlotPowerOff = getArg(request, "irSlotPowerOff").toInt();
    }

    autoMgr->setConfig(enabled, targetTemp, irSlotPowerOn, irSlotPowerOff);
    request->send(200, "application/json", "{\"success\":true}");
}

#endif
