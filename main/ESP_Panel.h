#pragma once

class ESP_Panel {  // we use two semaphores to sync the VSYNC event and the LVGL task, to avoid potential tearing effect
    enum class DisplayState { On, Off, PendingOn, PendingOnFromTouch, PendingOff, OnPendingRelease };

    static ESP_Panel *_instance;

    static bool on_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data,
                               void *user_data);
    static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
    static void increase_lvgl_tick(void *arg);

    esp_lcd_panel_handle_t _panel_handle;
    esp_lcd_touch_handle_t _touch_handle;
    esp_timer_handle_t _displayOffTimer;
    DisplayState _displayState;
    uint32_t _lastBacklightChange;
    Mutex _i2cMutex;

#if CONFIG_DISPLAY_AVOID_TEAR_EFFECT_WITH_SEM
    SemaphoreHandle_t sem_vsync_end;
    SemaphoreHandle_t sem_gui_ready;
#endif

public:
    ESP_Panel();

    lv_disp_t *begin(bool silent);
    void process();
    void displayOn();
    void displayOff();

private:
    esp_err_t i2c_master_init();
    void resetDisplayOffTimer();
    void handleDisplayState();
    void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data);
};
