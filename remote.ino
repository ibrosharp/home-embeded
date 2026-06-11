#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <WiFi.h>
#include <WebServer.h>

#include "SmartWebServer.hpp"
#include "TaskQueueManager.hpp"
#include "LoggingTask.hpp"
#include "StorageManager.hpp"
#include "HardwareManager.hpp"
#include "NetworkTask.hpp"
#include "QueueWorkerTask.hpp"
#include "SwitchPollerTask.hpp"
#include "SensorPollerTask.hpp"
#include "Room.hpp"
#include "Relay.hpp"
#include "switch.hpp"
#include "ClimateSensor.hpp"
#include "PresenceSensor.hpp"
#include "IRController.hpp"
#include "IRListenerTask.hpp"

#define IR_RECEIVE_PIN  7
#define IR_SEND_PIN    10
#define PIR_PIN         4
#define DHTPIN          6
#define I2C_SDA         1
#define I2C_SCL         3
#define INT_A_PIN      18
#define INT_B_PIN      19
#define LED_INDICATOR   5

#define MCP_RELAY_1    0  //GPA0
#define MCP_RELAY_2    1  //GPA1
#define MCP_SWITCH_3    2  //GPA2
#define MCP_SWITCH_4    3  //GPA3
#define MCP_SWITCH_5    4  //GPA4
#define MCP_SWITCH_6    5  //GPA5
#define MCP_RELAY_4    6  //GPA6
#define MCP_RELAY_3    7  //GPA7
#define MCP_SWITCH_9    8  //GPB0
#define MCP_SWITCH_10   9  //GPB1
#define MCP_SWITCH_11   10 //GPB2
#define MCP_SWITCH_12   11 //GPB3
#define MCP_SWITCH_1    12 //GPB4
#define MCP_SWITCH_2     13 //GPB5
#define MCP_SWITCH_8     14 //GPB6
#define MCP_SWITCH_7     15 //GPB7

TaskHandle_t WebTaskHandle = NULL;

// // Global Instances matching the extern setups
StorageManager globalStorage("sys_config");
SmartWebServer myWebServer(globalStorage, 80);
TaskQueueManager sysQueue(15);
IRCaptureRequest irCaptureRequest;

static inline void logDebug(const char* tag, const String& message) {
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, tag, message));
}

static inline void logInfo(const char* tag, const String& message) {
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, tag, message));
}

static inline void logWarn(const char* tag, const String& message) {
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::WARNING, tag, message));
}

static inline void logError(const char* tag, const String& message) {
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::ERROR, tag, message));
}

#define DHTTYPE DHT22
#define LIGHT_SENSOR_PIN 0

HardwareManager& hwInstance = HardwareManager::getInstance();

static Relay relays[4] = {
    Relay(hwInstance, MCP_RELAY_1, "relay_1", globalStorage),
    Relay(hwInstance, MCP_RELAY_2, "relay_2", globalStorage),
    Relay(hwInstance, MCP_RELAY_4, "relay_4", globalStorage),
    Relay(hwInstance, MCP_RELAY_3, "relay_3", globalStorage)
};

static Switch switches[12] = {
    Switch(hwInstance, MCP_SWITCH_1, "switch_1", globalStorage),
    Switch(hwInstance, MCP_SWITCH_2, "switch_2", globalStorage),
    Switch(hwInstance, MCP_SWITCH_3, "switch_3", globalStorage),
    Switch(hwInstance, MCP_SWITCH_4, "switch_4", globalStorage),
    Switch(hwInstance, MCP_SWITCH_5, "switch_5", globalStorage),
    Switch(hwInstance, MCP_SWITCH_6, "switch_6", globalStorage),
    Switch(hwInstance, MCP_SWITCH_7, "switch_7", globalStorage),
    Switch(hwInstance, MCP_SWITCH_8, "switch_8", globalStorage),
    Switch(hwInstance, MCP_SWITCH_9, "switch_9", globalStorage),
    Switch(hwInstance, MCP_SWITCH_10, "switch_10", globalStorage),
    Switch(hwInstance, MCP_SWITCH_11, "switch_11", globalStorage),
    Switch(hwInstance, MCP_SWITCH_12, "switch_12", globalStorage)
};

ClimateSensor climateSensor(DHTPIN, DHTTYPE);
IRController irController(IR_RECEIVE_PIN, IR_SEND_PIN, &globalStorage);
PresenceSensor presenceSensor(PIR_PIN);
LightSensor lightSensor(LIGHT_SENSOR_PIN);
Setting roomSettings("room_settings", &globalStorage);

void setup() {
    Serial.begin(115200);
    Serial.println("Serial console initialized");

    if (!globalStorage.begin()) {
        Serial.println("Failed to initialize Preferences storage");
        Serial.println("[FATAL] Storage begin failed.");
        while (1) { vTaskDelay(1); }
    }
    Serial.println("Preferences storage ready");
    roomSettings.load(globalStorage.prefs());
    irController.load(globalStorage.prefs());
    
    HardwareManager& hw = HardwareManager::getInstance();
    Serial.println( "Starting I2C hardware manager");
    if (!hw.begin(I2C_SDA, I2C_SCL, 0x20)) {
        Serial.println("[SYS] Notice: Continuing setup without physical port expander.");
    } else {
        Serial.println("[SYS] Hardware expander connected and online.");
    }
    Serial.println("Hardware manager initialized successfully");

    Serial.println("Attaching switches to relays and registering devices");
    // Attach the first four switches to the first four relays by default (no write to flash on boot)
    switches[0].attachRelay(&relays[0], false);
    switches[1].attachRelay(&relays[1], false);
    switches[2].attachRelay(&relays[2], false);
    switches[3].attachRelay(&relays[3], false);

    Room& room = Room::getInstance();
    room.registerClimateSensor(climateSensor);
    room.registerPresenceSensor(presenceSensor);
    room.registerLightSensor(lightSensor);
    room.registerIRController(irController);
    room.registerSetting(roomSettings);

    climateSensor.init();
    presenceSensor.init();
    lightSensor.init();
    irController.init();
    logDebug("LIGHT", String("Light sensor pointer is ") + (room.getLightSensor() != nullptr ? "valid" : "null") + " pin=" + String(LIGHT_SENSOR_PIN));

    for (int i = 0; i < 4; ++i) {
        logDebug("RELAY", "Beginning relay " + String(i) + " state=" + String(relays[i].getState()));
        relays[i].begin();
        room.registerRelay(&relays[i]);
    }

    for (int i = 0; i < 12; ++i) {
        logDebug("SWITCH", "Beginning switch " + String(i) + " state=" + String(switches[i].getState()));
        switches[i].begin();
        if (switches[i].hasLoadedRelayPin()) {
            uint8_t rPin = switches[i].getLoadedRelayPin();
            if (rPin == 255) {
                switches[i].attachRelay(nullptr, false);
            } else {
                Relay* r = room.findRelayByPin(rPin);
                switches[i].attachRelay(r, false);
            }
        }
        room.registerSwitch(&switches[i]);
    }

    // All MCP switch pins are pulled with resistors on the board, so once
    // the MCP GPIO pin mode is configured as INPUT it will not be read as
    // floating before the switch is pressed.
    Serial.println("Launching web server, queue worker, and switch poller tasks");
    BaseType_t webStatus = xTaskCreate(
        WebServerTask, "Web_Task", 8192, NULL, 1, &WebTaskHandle
    );

    if (webStatus != pdPASS) {
        Serial.println("Initial Web_Task creation failed, retrying with smaller stack");
        xTaskCreate(WebServerTask, "Web_Task", 4096, NULL, 1, &WebTaskHandle);
    }

    if (xTaskCreate(QueueWorkerTask, "Queue_Worker", 2048, NULL, 2, NULL) != pdPASS) {
        Serial.println("Failed to create Queue_Worker task");
    } else {
        Serial.println("Queue worker task started");
    }

    // Low-priority IR capture task listens for a pending capture request and
    // stores the decoded signal in the IR controller database.
    if (xTaskCreate(IRListenerTask, "IR_Listener", 2048, NULL, 0, NULL) != pdPASS) {
        Serial.println("Failed to create IR_Listener task");
    } else {
        Serial.println("IR listener task started");
    }

    // Priority 3 — higher than web (1) and queue worker (2) so physical
    // switch presses are never delayed by HTTP or log processing.
    if (xTaskCreate(SwitchPollerTask, "Switch_Poller", 2048, NULL, 3, NULL) != pdPASS) {
        Serial.println("Failed to create Switch_Poller task");
    } else {
        Serial.println("Switch poller task started");
    }

    // Priority 1 — lowest, same as the web server. Environmental data only
    // needs to be fresh within ~500 ms; it must not starve switches or logging.
    if (xTaskCreate(SensorPollerTask, "Sensor_Poller", 3072, NULL, 1, NULL) != pdPASS) {
        Serial.println("Failed to create Sensor_Poller task");
    } else {
        Serial.println("Sensor poller task started");
    }
}

void triggerFeedback() {
    Room::getInstance().triggerFeedback();
}

void loop() {
    Room& room = Room::getInstance();
    if (room.checkAndClearFeedback()) {
        int currentState = digitalRead(LED_INDICATOR);
        digitalWrite(LED_INDICATOR, currentState == HIGH ? LOW : HIGH);
        vTaskDelay(pdMS_TO_TICKS(150));
        digitalWrite(LED_INDICATOR, currentState);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
}



