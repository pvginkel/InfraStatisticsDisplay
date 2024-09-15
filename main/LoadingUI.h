#pragma once

#include "Callback.h"
#include "LvglUI.h"

enum class LoadingUIState { None, Loading, Error };

class LoadingUI : public LvglUI {
    const char* _title;
    const char* _error;
    Callback<void> _retryClicked;
    LoadingUIState _state;
    vector<lv_obj_t*> _loadingCircles;
    bool _silent;
#ifndef LV_SIMULATOR
    esp_timer_handle_t _restartTimer;
#endif

public:
    LoadingUI(lv_obj_t* parent, bool silent);
    ~LoadingUI() override;

    void setState(LoadingUIState state) { _state = state; }
    void setTitle(const char* title) { _title = title; }
    void setError(const char* error) { _error = error; }
    void redraw() { render(); }
    void onRetryClicked(function<void()> func) { _retryClicked.add(func); }

protected:
    void doRender(lv_obj_t* parent) override;

private:
    void resetRender();
    void renderTitle(lv_obj_t* parent, double offsetY);
    void renderLoading(lv_obj_t* parent);
    static void loadingAnimationCallback(void* var, int32_t v);
    void renderError(lv_obj_t* parent);
};
