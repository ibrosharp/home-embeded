#ifndef ERROR_MANAGER_HPP
#define ERROR_MANAGER_HPP

#include <Arduino.h>
#include <vector>

enum ErrorCode {
    ERROR_WIFI_CONNECT = 12,
    ERROR_STORAGE_INIT = 13,
    ERROR_I2C_EXPANDER = 21,
    ERROR_DHT_SENSOR = 31,
    ERROR_IR_RECEIVER = 41,
    ERROR_SYSTEM_MEMORY = 51
};

class ErrorManager {
private:
    std::vector<ErrorCode> _activeErrors;

    // Blink Engine State
    enum BlinkState {
        STATE_IDLE,
        STATE_PRE_PAUSE,
        STATE_TENS_BLINK_ON,
        STATE_TENS_BLINK_OFF,
        STATE_MID_PAUSE,
        STATE_ONES_BLINK_ON,
        STATE_ONES_BLINK_OFF,
        STATE_POST_PAUSE
    };

    BlinkState _state;
    unsigned long _lastTransition;
    size_t _currentErrorIndex;
    int _currentDigitCount;
    int _targetDigitCount;
    
    // Timing constants (ms)
    const unsigned long T_PRE_PAUSE = 2000;
    const unsigned long T_MID_PAUSE = 1000;
    const unsigned long T_POST_PAUSE = 2000;
    const unsigned long T_SLOW_ON = 600;
    const unsigned long T_SLOW_OFF = 400;
    const unsigned long T_FAST_ON = 200;
    const unsigned long T_FAST_OFF = 200;

    ErrorManager() {
        _state = STATE_IDLE;
        _lastTransition = 0;
        _currentErrorIndex = 0;
    }

public:
    static ErrorManager& getInstance() {
        static ErrorManager instance;
        return instance;
    }

    void setError(ErrorCode code) {
        for (auto c : _activeErrors) {
            if (c == code) return; // already exists
        }
        _activeErrors.push_back(code);
        
        // Kickstart engine if it was idle
        if (_state == STATE_IDLE && _activeErrors.size() == 1) {
            _currentErrorIndex = 0;
            _state = STATE_PRE_PAUSE;
            _lastTransition = millis();
        }
    }

    void clearError(ErrorCode code) {
        for (auto it = _activeErrors.begin(); it != _activeErrors.end(); ++it) {
            if (*it == code) {
                _activeErrors.erase(it);
                break;
            }
        }
        if (_activeErrors.empty()) {
            _state = STATE_IDLE;
        } else if (_currentErrorIndex >= _activeErrors.size()) {
            _currentErrorIndex = 0;
        }
    }

    bool hasErrors() const {
        return !_activeErrors.empty();
    }

    // Call this inside the main loop
    // Returns true if the LED should be ON due to error blinking, false if OFF
    // If it returns false and hasErrors() is false, the system can use the LED normally.
    void processLED(uint8_t ledPin, bool feedbackEnabled) {
        if (!feedbackEnabled || _activeErrors.empty()) {
            if (_state != STATE_IDLE) {
                _state = STATE_IDLE;
                digitalWrite(ledPin, LOW); // Safe default when clearing
            }
            return;
        }

        unsigned long now = millis();
        unsigned long elapsed = now - _lastTransition;

        ErrorCode currentError = _activeErrors[_currentErrorIndex];
        int tens = currentError / 10;
        int ones = currentError % 10;

        switch (_state) {
            case STATE_IDLE:
                // Should not happen if _activeErrors is not empty, but just in case
                if (!_activeErrors.empty()) {
                    _state = STATE_PRE_PAUSE;
                    _lastTransition = now;
                    digitalWrite(ledPin, LOW);
                }
                break;

            case STATE_PRE_PAUSE:
                digitalWrite(ledPin, LOW);
                if (elapsed >= T_PRE_PAUSE) {
                    _currentDigitCount = 0;
                    _targetDigitCount = tens;
                    _state = _targetDigitCount > 0 ? STATE_TENS_BLINK_ON : STATE_MID_PAUSE;
                    _lastTransition = now;
                }
                break;

            case STATE_TENS_BLINK_ON:
                digitalWrite(ledPin, HIGH);
                if (elapsed >= T_SLOW_ON) {
                    _state = STATE_TENS_BLINK_OFF;
                    _lastTransition = now;
                }
                break;

            case STATE_TENS_BLINK_OFF:
                digitalWrite(ledPin, LOW);
                if (elapsed >= T_SLOW_OFF) {
                    _currentDigitCount++;
                    if (_currentDigitCount >= _targetDigitCount) {
                        _state = STATE_MID_PAUSE;
                    } else {
                        _state = STATE_TENS_BLINK_ON;
                    }
                    _lastTransition = now;
                }
                break;

            case STATE_MID_PAUSE:
                digitalWrite(ledPin, LOW);
                if (elapsed >= T_MID_PAUSE) {
                    _currentDigitCount = 0;
                    _targetDigitCount = ones;
                    _state = _targetDigitCount > 0 ? STATE_ONES_BLINK_ON : STATE_POST_PAUSE;
                    _lastTransition = now;
                }
                break;

            case STATE_ONES_BLINK_ON:
                digitalWrite(ledPin, HIGH);
                if (elapsed >= T_FAST_ON) {
                    _state = STATE_ONES_BLINK_OFF;
                    _lastTransition = now;
                }
                break;

            case STATE_ONES_BLINK_OFF:
                digitalWrite(ledPin, LOW);
                if (elapsed >= T_FAST_OFF) {
                    _currentDigitCount++;
                    if (_currentDigitCount >= _targetDigitCount) {
                        _state = STATE_POST_PAUSE;
                    } else {
                        _state = STATE_ONES_BLINK_ON;
                    }
                    _lastTransition = now;
                }
                break;

            case STATE_POST_PAUSE:
                digitalWrite(ledPin, LOW);
                if (elapsed >= T_POST_PAUSE) {
                    // Move to next error
                    _currentErrorIndex++;
                    if (_currentErrorIndex >= _activeErrors.size()) {
                        _currentErrorIndex = 0;
                    }
                    _state = STATE_PRE_PAUSE;
                    _lastTransition = now;
                }
                break;
        }
    }
};

#endif // ERROR_MANAGER_HPP
