#pragma once

#include "Callback.h"
#include "LvglUI.h"

enum class LoadingUIState { None, Loading, Error };

class LoadingUI : public LvglUI {
    const char* _title;
    const char* _error;
    LoadingUIState _state;
    bool _silent;
#ifndef LV_SIMULATOR
    esp_timer_handle_t _restart_timer;
#endif

public:
    LoadingUI(bool silent);
    ~LoadingUI() override;

    void set_state(LoadingUIState state) { _state = state; }
    void set_title(const char* title) { _title = title; }
    void set_error(const char* error) { _error = error; }

protected:
    void do_render(lv_obj_t* parent) override;

private:
    void reset_render();
    void render_title(lv_obj_t* parent, double offsetY);
    void render_loading(lv_obj_t* parent);
    void render_error(lv_obj_t* parent);
};
