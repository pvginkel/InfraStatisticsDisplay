#include "includes.h"

MutexLock::MutexLock(Mutex* mutex) : _mutex(mutex) {}

MutexLock::~MutexLock() { _mutex->give(); }

Mutex::Mutex() : _lock(xSemaphoreCreateMutex()) {}

Mutex::~Mutex() { vSemaphoreDelete(_lock); }

MutexLock Mutex::take(TickType_t xTicksToWait) {
    xSemaphoreTake(_lock, xTicksToWait);
    return {this};
}

void Mutex::give() { xSemaphoreGive(_lock); }
