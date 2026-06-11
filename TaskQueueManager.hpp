#ifndef TASK_QUEUE_MANAGER_H
#define TASK_QUEUE_MANAGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "Task.hpp"

class TaskQueueManager {
private:
    QueueHandle_t _queueHandle;
    const int _queueSize;

public:
    TaskQueueManager(int queueSize = 20);
    bool begin();
    bool push(Task* task);
    void processNext();
};

// Implementation

TaskQueueManager::TaskQueueManager(int queueSize) : _queueSize(queueSize) {
    _queueHandle = xQueueCreate(_queueSize, sizeof(Task*));
}

bool TaskQueueManager::push(Task* task) {
    if (_queueHandle == nullptr || task == nullptr) return false;
    BaseType_t result = xQueueSendToBack(_queueHandle, &task, 0);
    return (result == pdPASS);
}

void TaskQueueManager::processNext() {
    if (_queueHandle == nullptr) return;
    Task* incomingTask = nullptr;
    if (xQueueReceive(_queueHandle, &incomingTask, portMAX_DELAY) == pdPASS) {
        if (incomingTask != nullptr) {
            incomingTask->execute();
            delete incomingTask; 
        }
    }
}

#endif
