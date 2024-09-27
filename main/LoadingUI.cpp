#include "includes.h"

#include "LoadingUI.h"

#include "Messages.h"
#include "lv_support.h"

LOG_TAG(LoadingUI);

LoadingUI::LoadingUI(bool silent)
    : _title(),
      _error(),
      _state(),
      _silent(silent)
#ifndef LV_SIMULATOR
      ,
      _restart_timer(nullptr)
#endif
{
}

LoadingUI::~LoadingUI() { reset_render(); }

void LoadingUI::do_render(lv_obj_t* parent) {
    reset_render();

    switch (_state) {
        case LoadingUIState::Loading:
            render_loading(parent);
            break;
        case LoadingUIState::Error:
            render_error(parent);
            break;
    }
}

void LoadingUI::reset_render() {
#ifndef LV_SIMULATOR

    if (_restart_timer) {
        ESP_ERROR_CHECK(esp_timer_delete(_restart_timer));
        _restart_timer = nullptr;
    }

#endif
}

void LoadingUI::render_title(lv_obj_t* parent, double offsetY) {
    auto label = lv_label_create(parent);
    lv_label_set_text(label, _title);
    lv_obj_set_style_text_font(label, NORMAL_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_bounds(label, pw(50), ph(offsetY), pw(90), ph(10), LV_TEXT_ALIGN_CENTER);
}

void LoadingUI::render_loading(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Rendering loading '%s'", _title);

    if (_silent) {
        return;
    }

    render_title(parent, 19);
}

void LoadingUI::render_error(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Rendering error '%s'", _error);

#ifndef LV_SIMULATOR

    const esp_timer_create_args_t restartTimerArgs = {
        .callback =
            [](void* arg) {
                ESP_LOGI(TAG, "Restarting in error screen");
                esp_restart();
            },
        .arg = this,
        .name = "restartTimer",
    };

    ESP_ERROR_CHECK(esp_timer_create(&restartTimerArgs, &_restart_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(_restart_timer, ESP_TIMER_SECONDS(CONFIG_DEVICE_RESTART_ON_FAILURE_INTERVAL)));

#endif

    render_title(parent, 13);

    auto error = lv_label_create(parent);
    lv_label_set_text(error, _error);
    lv_obj_set_style_text_font(error, NORMAL_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_align(error, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_bounds(error, pw(50), ph(34), pw(90), ph(30), LV_TEXT_ALIGN_CENTER);
}
