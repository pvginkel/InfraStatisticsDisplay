#include "includes.h"

#include "Application.h"

#include <chrono>

#include "Messages.h"
#include "driver/i2c.h"

LOG_TAG(Application);

Application::Application(Device* device) : _device(device) {}

void Application::do_begin() {
    ESP_LOGI(TAG, "Starting UI worker task");

    ESP_ERROR_ASSERT(xTaskCreatePinnedToCore([](void* param) { ((Application*)param)->run(); }, "Application::run_task",
                                             8192, this, 1, nullptr, 1));

    get_mqtt_connection().on_connected_changed([this](auto state) {
        if (state.connected) {
            state_changed();

            register_mqtt_callbacks();
        }
    });
}

void Application::register_mqtt_callbacks() {
    get_mqtt_connection().publish_button_discovery(
        {
            .name = "Identify",
            .object_id = "identify",
            .entity_category = "config",
            .device_class = "identify",
        },
        []() { ESP_LOGI(TAG, "Requested identification"); });

    get_mqtt_connection().publish_button_discovery(
        {
            .name = "Restart",
            .object_id = "restart",
            .entity_category = "config",
            .device_class = "restart",
        },
        []() {
            ESP_LOGI(TAG, "Requested restart");

            esp_restart();
        });
}

void Application::run() {
    ESP_LOGI(TAG, "Setting up loading UI");

    _loading_ui = new LoadingUI(is_silent_startup());

    _loading_ui->begin();
    _loading_ui->set_title(MSG_STARTING);
    _loading_ui->set_state(LoadingUIState::Loading);
    _loading_ui->render();

    auto last_tick_call = chrono::high_resolution_clock::now();

    while (true) {
        auto start = chrono::high_resolution_clock::now();

        process();

        lv_timer_handler();

        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

        auto sleep = 10 - duration;
        if (sleep > 0) {
            vTaskDelay(pdMS_TO_TICKS(sleep));
        }

        auto after_sleep = chrono::high_resolution_clock::now();
        auto last_tick_duration = chrono::duration_cast<chrono::milliseconds>(after_sleep - last_tick_call).count();

        lv_tick_inc(last_tick_duration);

        last_tick_call = after_sleep;
    }
}

void Application::do_network_connection_failed() {
    if (_loading_ui) {
        _loading_ui->set_error(MSG_FAILED_TO_CONNECT);
        _loading_ui->set_state(LoadingUIState::Error);
        _loading_ui->render();
    }
}

void Application::do_ready() {
    delete _loading_ui;
    _loading_ui = nullptr;

    _stats_ui = new StatsUI();
    _stats_ui->begin();
}

void Application::do_process() {
    _device->process();

    if (_stats_ui) {
        _stats_ui->update();
    }
}

void Application::state_changed() {
    if (!get_mqtt_connection().is_connected()) {
        return;
    }

    get_mqtt_connection().send_state();
}
