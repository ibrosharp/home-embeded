#ifndef SWITCH_POLLER_TASK_HPP
#define SWITCH_POLLER_TASK_HPP

#include <Arduino.h>
#include "Room.hpp"

// High-priority FreeRTOS task that polls every registered switch for pin
// state changes. When a switch detects a debounced edge it will automatically
// toggle any attached relay via Switch::update().
//
// Priority should be set HIGHER than the web-server task so that physical
// input is never starved by HTTP traffic.
inline void SwitchPollerTask(void* pvParameters) {
    Serial.println("[SWITCH POLLER] Switch polling task started.");

    for (;;) {
        Room& room = Room::getInstance();

        for (int i = 0; i < MAX_SWITCHES; i++) {
            Switch* sw = room.getSwitch(i);
            if (sw != nullptr) {
                sw->update();
            }
        }

        // 10 ms tick — tight enough to catch a 50 ms debounce window reliably
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

#endif // SWITCH_POLLER_TASK_HPP
