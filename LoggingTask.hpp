#ifndef LOGGING_TASK_HPP
#define LOGGING_TASK_HPP

#include <Arduino.h>
#include "Task.hpp" // Matches your Task.hpp casing

namespace Firmware {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class LoggingTask : public Task {
private:
    LogLevel _level;
    const char* _tag;
    String _message; // Using String here to dynamically preserve the text until execution
    uint32_t _timestamp;

    const char* getLevelString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG:   return "DEBUG";
            case LogLevel::INFO:    return "INFO";
            case LogLevel::WARNING: return "WARN";
            case LogLevel::ERROR:   return "ERROR";
            default:                return "UNKN";
        }
    }

public:
    // Constructor instantiates a single immutable log command
    LoggingTask(LogLevel level, const char* tag, String message) 
        : _level(level), _tag(tag), _message(message), _timestamp(millis()) {}

    // Overridden execute pattern matching your RelayToggleTask design
    void execute() override {
        // This is executed safely and sequentially by your central queue processor task
        Serial.printf("[%6u] [%s] (%s): %s\n", 
                      _timestamp, 
                      getLevelString(_level), 
                      _tag, 
                      _message.c_str());
    }
};

} // namespace Firmware

#endif