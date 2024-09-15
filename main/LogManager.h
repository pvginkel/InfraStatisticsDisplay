#pragma once

#include "DeviceConfiguration.h"

class LogManager {
    struct Message {
        char* buffer;
        uint32_t time;

        Message(char* buffer, uint32_t time) : buffer(buffer), time(time) {}
    };

    static LogManager* _instance;
    static char* _buffer;

    vprintf_like_t _defaultLogHandler;
    Mutex _mutex;
    vector<Message> _messages;
    const DeviceConfiguration* _configuration;
    esp_timer_handle_t _logTimer;

    static int logHandler(const char* message, va_list va);

public:
    LogManager();

    void begin();
    void setConfiguration(const DeviceConfiguration& configuration);

private:
    void uploadLogs();
    void startTimer();
};
