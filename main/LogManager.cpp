#include "includes.h"

#include "LogManager.h"

constexpr auto BUFFER_SIZE = 1024;
constexpr auto BLOCK_SIZE = 10;

static const char* TAG = "LogManager";

LogManager* LogManager::_instance = nullptr;
char* LogManager::_buffer = new char[BUFFER_SIZE];

int LogManager::logHandler(const char* message, va_list va) {
    va_list vaCopy;
    va_copy(vaCopy, va);

    _instance->_defaultLogHandler(message, vaCopy);

    va_end(vaCopy);

    return _instance->_mutex.with<int>([message, va]() {
        auto result = vsnprintf(_buffer, BUFFER_SIZE, message, va);

        bool startTimer = false;

        if (result >= 0 && result < BUFFER_SIZE) {
            startTimer = _instance->_configuration && _instance->_messages.size() == 0;

            _instance->_messages.push_back(Message(strdup(_buffer), esp_get_millis()));
        }

        if (startTimer) {
            _instance->startTimer();
        }

        return result;
    });
}

LogManager::LogManager() : _defaultLogHandler(nullptr), _configuration(nullptr) { _instance = this; }

void LogManager::begin() {
    _defaultLogHandler = esp_log_set_vprintf(logHandler);

    const esp_timer_create_args_t displayOffTimerArgs = {
        .callback = [](void* arg) { ((LogManager*)arg)->uploadLogs(); },
        .arg = this,
        .name = "logManagerTimer",
    };

    ESP_ERROR_CHECK(esp_timer_create(&displayOffTimerArgs, &_logTimer));

    esp_register_shutdown_handler([]() {
        if (_instance) {
            ESP_LOGI(TAG, "Uploading log messages before restart");

            _instance->uploadLogs();
        }
    });
}

void LogManager::setConfiguration(const DeviceConfiguration& configuration) {
    auto startTimer = _mutex.with<bool>([this, &configuration]() {
        _configuration = &configuration;

        return _messages.size() > 0;
    });

    if (startTimer) {
        this->startTimer();
    }
}

void LogManager::uploadLogs() {
    auto messages = _mutex.with<vector<Message>>([this]() {
        if (!_configuration) {
            return vector<Message>();
        }

        auto messages = _messages;

        _messages.clear();

        return messages;
    });

    string buffer;
    auto offset = 0;

    while (offset < messages.size()) {
        buffer.clear();

        auto millis = esp_get_millis();

        for (auto i = 0; i < BLOCK_SIZE; i++) {
            auto index = offset++;
            if (index >= messages.size()) {
                break;
            }

            auto message = messages[index];

            auto root = cJSON_CreateObject();

            cJSON_AddStringToObject(root, "message", message.buffer);
            cJSON_AddNumberToObject(root, "relative_time", millis - message.time);
            cJSON_AddStringToObject(root, "entity_id", _configuration->getDeviceEntityId().c_str());

            auto json = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);

            buffer.append(json);
            buffer.append("\n");

            cJSON_free(json);

            free(message.buffer);
        }

        esp_http_client_config_t config = {
            .url = CONFIG_LOG_ENDPOINT,
            .timeout_ms = CONFIG_LOG_RECV_TIMEOUT,
        };

        auto err = esp_http_upload_string(config, buffer.c_str());
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to upload log: %d", err);
        }
    }
}

void LogManager::startTimer() { ESP_ERROR_CHECK(esp_timer_start_once(_logTimer, ESP_TIMER_MS(CONFIG_LOG_INTERVAL))); }
