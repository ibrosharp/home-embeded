#ifndef SAVE_CONFIG_TASK_H
#define SAVE_CONFIG_TASK_H

#include "Task.hpp"
#include "StorageManager.hpp"
#include <Arduino.h>

class SaveConfigTask : public Task {
private:
    StorageManager& _storage;
    String _ssid;
    String _pass;

public:
    SaveConfigTask(StorageManager& storage, String ssid, String pass) 
        : _storage(storage), _ssid(ssid), _pass(pass) {}

    void execute() override {
        _storage.saveWifiCredentials(_ssid, _pass);
    }
};

#endif
