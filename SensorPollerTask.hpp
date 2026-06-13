#ifndef SENSOR_POLLER_TASK_HPP
#define SENSOR_POLLER_TASK_HPP

#include <Arduino.h>
#include "Room.hpp"

// Low-priority FreeRTOS task that periodically samples all environmental
// sensors (climate, presence, light) and updates their internal cached state.
//
// Reading sensors is not time-critical so this runs at priority 1 (same as
// the web server) with a 500 ms tick — well within the 2 s DHT22 sample
// budget and generous for a PIR / ADC read.
inline void SensorPollerTask(void* pvParameters) {
    Serial.println("[SENSOR POLLER] Sensor polling task started.");
    sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::DEBUG, "SENSOR", "SensorPollerTask started"));

    for (;;) {
        Room& room = Room::getInstance();

        // --- Climate (temperature + humidity) ---
        ClimateSensor* climate = room.getClimateSensor();
        if (climate != nullptr) {
            climate->update();
            
            ClimateAutomation* climateAuto = room.getClimateAutomation();
            if (climateAuto != nullptr) {
                climateAuto->evaluate(climate->getTemperature(), room);
            }
        }

        // --- Presence (PIR motion) ---
        PresenceSensor* presence = room.getPresenceSensor();
        if (presence != nullptr) {
            presence->update();
        }

        // --- Light level (LDR / ADC) ---
        LightSensor* light = room.getLightSensor();
        if (light != nullptr) {
            light->update();
            
            // --- Evaluate Light Automation ---
            LightAutomation* lightAuto = room.getLightAutomation();
            if (lightAuto != nullptr) {
                lightAuto->evaluate(light->getPercentage(), room);
            }
        }

        // --- History Buffer Sampling (Every 60s) ---
        HistoryBuffer* history = room.getHistoryBuffer();
        if (history != nullptr && climate != nullptr && light != nullptr) {
            if (millis() - history->getLastSampleTime() > 60000) {
                history->addPoint(climate->getTemperature(), climate->getHumidity(), light->getPercentage());
            }
        }

        // 500 ms between sensor sweeps. The DHT22 needs at least 2 s between
        // reads anyway; ClimateSensor::update() guards the rate internally so
        // calling it every 500 ms is safe.
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

#endif // SENSOR_POLLER_TASK_HPP
