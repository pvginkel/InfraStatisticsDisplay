#include "includes.h"

#include "Application.h"

#include "Messages.h"
#include "driver/i2c.h"
#include "esp_netif_sntp.h"

LOG_TAG(Application);

Application* Application::_instance = nullptr;

Application::Application(Device* device)
    : _device(device), _wifi_connection(&_queue), _loading_ui(nullptr), _stats_ui(nullptr) {
    _instance = this;
}

void Application::begin(bool silent) {
    ESP_LOGI(TAG, "Setting up the log manager");

    _log_manager.begin();

    setup_flash();
    do_begin(silent);
}

void Application::setup_flash() {
    ESP_LOGI(TAG, "Setting up flash");

    auto ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void Application::do_begin(bool silent) {
    ESP_LOGI(TAG, "Setting up loading UI");

    _loading_ui = new LoadingUI(silent);

    _loading_ui->begin();
    _loading_ui->set_title(MSG_STARTING);
    _loading_ui->set_state(LoadingUIState::Loading);
    _loading_ui->render();

    begin_wifi();
}

void Application::begin_wifi() {
    ESP_LOGI(TAG, "Connecting to WiFi");

    _wifi_connection.on_state_changed([this](auto state) {
        if (!_loading_ui) {
            esp_restart();
        }

        if (state.connected) {
            begin_wifi_available();
        } else {
            _loading_ui->set_error(MSG_FAILED_TO_CONNECT);
            _loading_ui->set_state(LoadingUIState::Error);
            _loading_ui->render();
        }
    });

    _wifi_connection.begin();
}

void Application::begin_wifi_available() {
    ESP_LOGI(TAG, "WiFi available, initializing SNTP");

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");

    config.sync_cb = [](struct timeval* tv) {
        tm time_info;
        localtime_r(&tv->tv_sec, &time_info);

        ESP_LOGI(TAG, "Time synchronized to %d-%d-%d %d:%02d:%02d", time_info.tm_year, time_info.tm_mon,
                 time_info.tm_mday, time_info.tm_hour, time_info.tm_min, time_info.tm_sec);

        _instance->_queue.enqueue([]() { _instance->begin_sntp_synced(); });
    };

    esp_netif_sntp_init(&config);
}

void Application::begin_sntp_synced() {
    ESP_LOGI(TAG, "Getting device configuration");

    auto err = _configuration.load();

    if (err != ESP_OK) {
        auto error = format(MSG_FAILED_TO_RETRIEVE_CONFIGURATION, _configuration.get_endpoint().c_str());

        _loading_ui->set_error(strdup(error.c_str()));
        _loading_ui->set_state(LoadingUIState::Error);
        _loading_ui->render();
        return;
    }

    _log_manager.set_configuration(_configuration);

    if (_configuration.get_enable_ota()) {
        _ota_manager.begin();
    }

    _queue.enqueue([this]() { begin_after_initialization(); });
}

void Application::begin_after_initialization() {
    // Intialization complete.

    delete _loading_ui;
    _loading_ui = nullptr;

    // Log the reset reason.
    auto reset_reason = esp_reset_reason();
    ESP_LOGI(TAG, "esp_reset_reason: %s (%d)", esp_reset_reason_to_name(reset_reason), reset_reason);

    begin_ui();
}

void Application::begin_ui() {
    ESP_LOGI(TAG, "Connected, showing UI");

    _stats_ui = new StatsUI();
    _stats_ui->begin();
}

void Application::process() {
    _device->process();

    _queue.process();

    if (_stats_ui) {
        _stats_ui->update();
    }
}
