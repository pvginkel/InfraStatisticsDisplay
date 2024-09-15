#include "includes.h"

#include "Queue.h"

Queue::Queue() { _queue = xQueueCreate(32, sizeof(function<void()>)); }

void Queue::enqueue(const function<void()> &task) { xQueueSend(_queue, &task, portMAX_DELAY); }

void Queue::process() {
    while (uxQueueMessagesWaiting(_queue) > 0) {
        function<void()> task;
        if (xQueueReceive(_queue, &task, 0) == pdTRUE) {
            task();
        }
    }
}
