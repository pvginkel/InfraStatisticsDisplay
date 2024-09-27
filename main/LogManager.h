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

    vprintf_like_t _default_log_handler;
    Mutex _mutex;
    vector<Message> _messages;
    const DeviceConfiguration* _configuration;
    esp_timer_handle_t _log_timer;

    static int log_handler(const char* message, va_list va);

public:
    LogManager();

    void begin();
    void set_configuration(const DeviceConfiguration& configuration);

private:
    void uploadLogs();
    void start_timer();
};
