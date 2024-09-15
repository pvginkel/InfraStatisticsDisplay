#pragma once

class Mutex;

class MutexLock {
    Mutex* _mutex;

    MutexLock(Mutex* mutex);
    MutexLock(const MutexLock&) = delete;
    MutexLock& operator=(const MutexLock&) = delete;
    MutexLock(MutexLock&&) = delete;
    MutexLock& operator=(MutexLock&&) = delete;

public:
    ~MutexLock();

    friend class Mutex;
};

class Mutex {
    SemaphoreHandle_t _lock;

public:
    Mutex();
    ~Mutex();

    [[nodiscard]] MutexLock take(TickType_t xTicksToWait = portMAX_DELAY);

    template <typename Result>
    Result with(function<Result(void)> func, TickType_t xTicksToWait = portMAX_DELAY) {
        auto lock = take(xTicksToWait);
        return func();
    }

    void with(function<void(void)> func, TickType_t xTicksToWait = portMAX_DELAY) {
        auto lock = take(xTicksToWait);
        func();
    }

private:
    void give();

    friend class MutexLock;
};
