#ifndef QUEUE_WORKER_TASK_HPP
#define QUEUE_WORKER_TASK_HPP

#include <Arduino.h>
#include "TaskQueueManager.hpp"

extern TaskQueueManager sysQueue;

// Marked inline so it compiles cleanly inside a single header file
inline void QueueWorkerTask(void* pvParameters) {
    Serial.println("[WORKER] Queue processing loop started.");
    for(;;) {
        sysQueue.processNext();
        vTaskDelay(pdMS_TO_TICKS(5)); 
    }
}

#endif