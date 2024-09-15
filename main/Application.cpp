#include "includes.h"

#include "Application.h"

#include "Messages.h"
#include "driver/i2c.h"

static const char *TAG = "Application";

Application::Application(ESP_Panel &panel)
    : _panel(panel),
      _parent(nullptr),
      _wifiConnection(&_queue),
      _mqttConnection(nullptr),
      _loadingUI(nullptr),
      _thermostatUI(nullptr),
      _motionSensor(&_queue) {}

void Application::begin(lv_disp_t *disp, bool silent) {
    ESP_LOGI(TAG, "Setting up the log manager");

    _logManager.begin();

    ESP_LOGI(TAG, "Setting up motion sensor");

    _motionSensor.onTriggered([this] {
        ESP_LOGI(TAG, "Turning display on because of motion");
        _panel.displayOn();
    });

    _motionSensor.begin();

    _parent = lv_disp_get_scr_act(disp);
    lv_obj_set_style_bg_color(_parent, lv_color_white(), 0);

    setupFlash();
    begin(silent);
}

void Application::setupFlash() {
    ESP_LOGI(TAG, "Setting up flash");

    auto ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void Application::begin(bool silent) {
    ESP_LOGI(TAG, "Setting up loading UI");

    _loadingUI = new LoadingUI(_parent, silent);

    _loadingUI->begin();
    _loadingUI->setTitle(Messages::connectingToWifi);
    _loadingUI->setState(LoadingUIState::Loading);
    _loadingUI->onRetryClicked([] { esp_restart(); });
    _loadingUI->redraw();

    beginWifi();
}

void Application::beginWifi() {
    ESP_LOGI(TAG, "Connecting to WiFi");

    _wifiConnection.onStateChanged([this](auto state) {
        if (!_loadingUI) {
            esp_restart();
        }

        if (state.connected) {
            beginWifiAvailable();
        } else {
            _loadingUI->setError(Messages::failedToConnect);
            _loadingUI->setState(LoadingUIState::Error);
            _loadingUI->redraw();
        }
    });

    _wifiConnection.begin();
}

void Application::beginWifiAvailable() {
    ESP_LOGI(TAG, "WiFi available, starting other services");

    auto err = _configuration.load();

    if (err != ESP_OK) {
        auto error = format(Messages::failedToRetrieveConfiguration, _configuration.getEndpoint().c_str());

        _loadingUI->setError(strdup(error.c_str()));
        _loadingUI->setState(LoadingUIState::Error);
        _loadingUI->redraw();

        return;
    }

    _logManager.setConfiguration(_configuration);

    if (_configuration.getEnableOTA()) {
        _otaManager.begin();

        // OTA writes a lot of data to flash. This causes screen drift because SPIRAM gets
        // disabled. To mitigate this, we turn off the screen when OTA starts.
        _otaManager.onOTAStart([this] { _panel.displayOff(); });
    }

    beginMQTT();
}

void Application::beginMQTT() {
    _loadingUI->setTitle(Messages::connectingToHomeAssistant);
    _loadingUI->redraw();

    ESP_LOGI(TAG, "Connecting to MQTT / Home Assistant");

    _mqttConnection = new MQTTConnection(&_queue, _configuration);

    _mqttConnection->onStateChanged([this](auto state) {
        if (!_loadingUI) {
            esp_restart();
        }

        if (state.connected) {
            delete _loadingUI;
            _loadingUI = nullptr;

            // Log the reset reason.
            auto resetReason = esp_reset_reason();
            ESP_LOGI(TAG, "esp_reset_reason: %s (%d)", esp_reset_reason_to_name(resetReason), resetReason);

            beginUI();
        } else {
            _loadingUI->setError(Messages::failedToConnect);
            _loadingUI->setState(LoadingUIState::Error);
            _loadingUI->redraw();
        }
    });

    _mqttConnection->begin();
}

void Application::beginUI() {
    ESP_LOGI(TAG, "Connected, showing UI");

    _thermostatUI = new ThermostatUI(_parent);

    _thermostatUI->onSetpointChanged([this](auto setpoint) {
        ESP_LOGI(TAG, "Sending new setpoint from the thermostat to MQTT");

        auto state = _mqttConnection->getState();
        state.setpoint = setpoint;
        _mqttConnection->setState(state);
    });

    _thermostatUI->onModeChanged([this](auto mode) {
        ESP_LOGI(TAG, "Sending new mode from the thermostat to MQTT");

        auto state = _mqttConnection->getState();
        state.mode = mode;
        _mqttConnection->setState(state);
    });

    _thermostatUI->begin();
    _thermostatUI->setState(_mqttConnection->getState());

    _mqttConnection->onThermostatStateChanged([this] {
        ESP_LOGI(TAG, "Sending new state from MQTT to the thermostat");

        auto oldState = _thermostatUI->getState();
        auto newState = _mqttConnection->getState();

        auto setpointChanged = oldState.setpoint != newState.setpoint;
        auto modeChanged = oldState.mode != newState.mode;
        auto stateChanged = oldState.state != newState.state;

        _thermostatUI->setState(newState);

        if (setpointChanged || modeChanged || stateChanged) {
            _panel.displayOn();
        }
    });
}

void Application::process() { _queue.process(); }
