#pragma once

class MotionSensor {
    Callback<void> _triggered;
    QueueHandle_t _queue;
    Queue *_synchronizationQueue;

public:
    MotionSensor(Queue *synchronizationQueue);
    void begin();
    void onTriggered(function<void()> func) { _triggered.add(func); }

private:
    void taskHandler();
    void interruptHandler();
};
