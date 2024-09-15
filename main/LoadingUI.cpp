#include "includes.h"

#include "LoadingUI.h"

#include "Messages.h"

#define SELF(e) ((LoadingUI*)lv_event_get_user_data((e)))

static const char* TAG = "LoadingUI";

constexpr auto CIRCLES = 11;
constexpr auto CIRCLES_RADIUS = 10;
constexpr auto CIRCLE_RADIUS = 4;

LoadingUI::LoadingUI(lv_obj_t* parent, bool silent)
    : LvglUI(parent), _title(), _error(), _state(), _silent(silent)
#ifndef LV_SIMULATOR
    , _restartTimer(nullptr)
#endif
{}

LoadingUI::~LoadingUI() { resetRender(); }

void LoadingUI::doRender(lv_obj_t* parent) {
    resetRender();

    switch (_state) {
        case LoadingUIState::Loading:
            renderLoading(parent);
            break;
        case LoadingUIState::Error:
            renderError(parent);
            break;
    }
}

void LoadingUI::resetRender() {
    lv_anim_del(this, loadingAnimationCallback);

    _loadingCircles.clear();

#ifndef LV_SIMULATOR

    if (_restartTimer) {
        ESP_ERROR_CHECK(esp_timer_delete(_restartTimer));
        _restartTimer = nullptr;
    }

#endif
}

void LoadingUI::renderTitle(lv_obj_t* parent, double offsetY) {
    auto label = lv_label_create(parent);
    lv_label_set_text(label, _title);
    lv_obj_set_style_text_font(label, NORMAL_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_bounds(label, pw(50), ph(offsetY), pw(90), ph(10), LV_TEXT_ALIGN_CENTER);
}

void LoadingUI::renderLoading(lv_obj_t* parent) {
    if (_silent) {
        return;
    }

    renderTitle(parent, 19);

    auto centerX = pw(50);
    auto centerY = ph(50);
    auto radius = pw(CIRCLES_RADIUS);

    for (auto i = 0; i < CIRCLES; i++) {
        auto obj = lv_obj_create(parent);
        _loadingCircles.push_back(obj);

        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(obj, lv_theme_get_color_primary(parent), 0);
        lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(obj, 0, 0);
        lv_obj_set_size(obj, pw(CIRCLE_RADIUS), pw(CIRCLE_RADIUS));

        auto angleRadians = (360.0 / CIRCLES * i) * (M_PI / 180);

        auto x = centerX + radius * cos(angleRadians);
        auto y = centerY + radius * sin(angleRadians);

        lv_obj_set_x(obj, int32_t(x));
        lv_obj_set_y(obj, int32_t(y));
    }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, this);
    lv_anim_set_values(&a, 0, CIRCLES);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_user_data(&a, this);

    lv_anim_set_exec_cb(&a, loadingAnimationCallback);

    lv_anim_start(&a);
}

void LoadingUI::loadingAnimationCallback(void* var, int32_t v) {
    const auto self = (LoadingUI*)var;

    for (const auto obj : self->_loadingCircles) {
        lv_obj_set_style_bg_opa(obj, (v / double(CIRCLES)) * 255, 0);
        v = (v - 1) % CIRCLES;
    }
}

void LoadingUI::renderError(lv_obj_t* parent) {
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

    ESP_ERROR_CHECK(esp_timer_create(&restartTimerArgs, &_restartTimer));
    ESP_ERROR_CHECK(esp_timer_start_once(_restartTimer, ESP_TIMER_SECONDS(CONFIG_DEVICE_RESTART_ON_FAILURE_INTERVAL)));

#endif

    renderTitle(parent, 13);

    auto error = lv_label_create(parent);
    lv_label_set_text(error, _error);
    lv_obj_set_style_text_font(error, NORMAL_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_align(error, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_bounds(error, pw(50), ph(34), pw(90), ph(30), LV_TEXT_ALIGN_CENTER);

    auto button = lv_btn_create(parent);
    lv_obj_set_style_text_font(button, NORMAL_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_align(button, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_bounds(button, pw(50), ph(60), pw(30), ph(14), LV_TEXT_ALIGN_CENTER);
    lv_obj_add_event_cb(
        button, [](auto e) { SELF(e)->_retryClicked.call(); }, LV_EVENT_CLICKED, this);

    auto buttonLabel = lv_label_create(button);
    lv_label_set_text(buttonLabel, Messages::retry);
    lv_obj_center(buttonLabel);
}
