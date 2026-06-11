#ifndef IR_TRANSMIT_TASK_H
#define IR_TRANSMIT_TASK_H

#include "Task.hpp"
#include "IRController.hpp"

// Enqueue one of these onto sysQueue to blast an IR slot from the
// QueueWorker thread. transmitRaw() already stops the receiver, fires
// the signal, then restarts the receiver — all thread-safely inside the
// worker context so the IR hardware is never touched from two tasks at once.
extern void triggerFeedback();

class IRTransmitTask : public Task {
private:
    IRController& _ir;
    int _slotIndex;

public:
    IRTransmitTask(IRController& ir, int slotIndex)
        : _ir(ir), _slotIndex(slotIndex) {}

    void execute() override {
        _ir.transmitSlot(_slotIndex);
        triggerFeedback();
    }
};

#endif // IR_TRANSMIT_TASK_H
