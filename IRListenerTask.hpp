#ifndef IR_LISTENER_TASK_HPP
#define IR_LISTENER_TASK_HPP

#include <Arduino.h>
#include <IRremote.h>
#include "Room.hpp"

extern void triggerFeedback();

// A shared volatile slot for the web API to request a capture.
// -1 means "no pending capture request".
// The web API writes the target slot + labels; the task reads and clears it.
struct IRCaptureRequest {
    volatile int    slot     = -1;
    volatile bool   pending  = false;
    char            deviceId[32] = {};
    char            name[32]    = {};
};

// Defined in remote.ino (extern declared here so both the task and the
// API handler can access it without including remote.ino).
extern IRCaptureRequest irCaptureRequest;

// Low-priority FreeRTOS task — polls the IR receiver every 50 ms.
// It continuously decodes and logs incoming IR signals.
// When a capture request is pending, it saves the next valid decoded signal.
inline void IRListenerTask(void* pvParameters) {
    Serial.println("[IR LISTENER] IR listener task started.");

    for (;;) {
        if (IrReceiver.decode()) {
            bool isRepeatSignal = (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT);

            // Log every decoded signal (not unknown, and not repeat)
            if (IrReceiver.decodedIRData.protocol != UNKNOWN && !isRepeatSignal) {
                String logMsg = String("[IR] Detected Signal: Proto=") + IrReceiver.decodedIRData.protocol + 
                                " Addr=0x" + String(IrReceiver.decodedIRData.address, HEX) + 
                                " Cmd=0x" + String(IrReceiver.decodedIRData.command, HEX) +
                                " Bits=" + IrReceiver.decodedIRData.numberOfBits;
                sysQueue.push(new Firmware::LoggingTask(Firmware::LogLevel::INFO, "IR", logMsg));
                Serial.println(logMsg);

                // If a capture request is pending, save the next valid IR command
                if (irCaptureRequest.pending) {
                    Room& room = Room::getInstance();
                    IRController* ir = room.getIRController();
                    if (ir != nullptr) {
                        int slot = irCaptureRequest.slot;
                        IRCommand cmd;
                        cmd.protocol = IrReceiver.decodedIRData.protocol;
                        cmd.address = IrReceiver.decodedIRData.address;
                        cmd.command = IrReceiver.decodedIRData.command;
                        cmd.numberOfBits = IrReceiver.decodedIRData.numberOfBits;
                        cmd.deviceId = String(irCaptureRequest.deviceId);
                        cmd.name = String(irCaptureRequest.name);
                        cmd.isValid = true;

                        ir->saveCustomCommand(slot, cmd);

                        Serial.printf("[IR LISTENER] Saved IR Signal to slot %d -> Device: %s, Name: %s\n", 
                                      slot, cmd.deviceId.c_str(), cmd.name.c_str());
                        
                        irCaptureRequest.pending = false;
                        irCaptureRequest.slot    = -1;
                        triggerFeedback();
                    }
                }
            }
            IrReceiver.resume(); // Clear buffer and wait for next signal
        }

        // 50 ms — tight enough to catch a full IR burst reliably
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

#endif // IR_LISTENER_TASK_HPP
