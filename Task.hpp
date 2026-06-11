#ifndef TASK_H
#define TASK_H

class Task {
public:
    // Virtual destructor is critical to ensure child classes clean up their memory safely
    virtual ~Task() {}
    
    // Pure virtual function that every specific task must implement
    virtual void execute() = 0;
};

#endif
