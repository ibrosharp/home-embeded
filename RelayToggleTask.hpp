#ifndef RELAY_TOGGLE_TASK_H
#define RELAY_TOGGLE_TASK_H

#include "Task.hpp"
#include "Relay.hpp"

class RelayToggleTask : public Task {
private:
    Relay& _relay;

public:
    RelayToggleTask(Relay& relayInstance) : _relay(relayInstance) {}

    void execute() override {
        _relay.toggle(); // Concrete action executed when pulled from the queue
    }
};

#endif
