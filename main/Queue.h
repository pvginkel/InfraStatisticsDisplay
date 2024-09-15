#pragma once

class Queue {
    QueueHandle_t _queue;

public:
    Queue();

    void enqueue(const function<void()> &task);
    void process();
};