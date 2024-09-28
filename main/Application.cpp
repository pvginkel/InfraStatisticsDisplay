#include "includes.h"

#include "Application.h"

#include "Messages.h"
#include "driver/i2c.h"

LOG_TAG(Application);

Application::Application(Device* device)
    : _device(device),
      _network_connection(&_queue),
      _loading_ui(nullptr),
      _stats_ui(nullptr),
      _have_sntp_synced(false) {}

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

    begin_network();
}

void Application::begin_network() {
    ESP_LOGI(TAG, "Connecting to WiFi");

    _network_connection.on_state_changed([this](auto state) {
        if (!_loading_ui) {
            esp_restart();
        }

        if (state.connected) {
            begin_network_available();
        } else {
            _loading_ui->set_error(MSG_FAILED_TO_CONNECT);
            _loading_ui->set_state(LoadingUIState::Error);
            _loading_ui->render();
        }
    });

    _network_connection.begin();
}

void Application::begin_network_available() {
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
