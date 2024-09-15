#include "includes.h"

#include "ESP_Panel.h"

#include "CH422G.h"
#include "ESP_Panel_Conf.h"
#include "I2CConf.h"

constexpr auto BACKLIGHT_CHANGE_DEBOUNCE = 1000;

// The sample uses 160 (1/3d of 480), but we don't have that much memory available.
#define DISPLAY_BUFFER_LINES (480 / 5)

#define LVGL_TICK_PERIOD_MS 2

#if CONFIG_DISPLAY_DOUBLE_FB
#define LCD_NUM_FB 2
#else
#define LCD_NUM_FB 1
#endif  // CONFIG_DISPLAY_DOUBLE_FB

ESP_Panel *ESP_Panel::_instance = nullptr;

static const char *TAG = "ESP_Panel";

ESP_Panel::ESP_Panel()
    : _panel_handle(nullptr),
      _touch_handle(nullptr),
      _displayOffTimer(nullptr),
      _displayState(DisplayState::Off),
      _lastBacklightChange(0) {
    _instance = this;
}

/**
 * @brief i2c master initialization
 */
esp_err_t ESP_Panel::i2c_master_init() {
    auto i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master =
            {
                .clk_speed = I2C_MASTER_FREQ_HZ,
            },
    };

    i2c_param_config(i2c_master_port, &conf);

    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

// extern lv_obj_t *scr;
void ESP_Panel::lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    bool touchpad_pressed = _i2cMutex.with<bool>([this, &touchpad_x, &touchpad_y, &touchpad_cnt]() {
        /* Read touch controller data */
        esp_lcd_touch_read_data(_touch_handle);

        /* Get coordinates */
        return esp_lcd_touch_get_coordinates(_touch_handle, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);
    });

    if (touchpad_pressed && touchpad_cnt > 0) {
        if (_displayState != DisplayState::On) {
            if (_displayState == DisplayState::Off) {
                _displayState = DisplayState::PendingOnFromTouch;
            }
            return;
        }

        resetDisplayOffTimer();

        data->point.x = touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PR;
    } else {
        if (_displayState == DisplayState::OnPendingRelease) {
            _displayState = DisplayState::On;

            resetDisplayOffTimer();
        }

        data->state = LV_INDEV_STATE_REL;
    }
}

bool ESP_Panel::on_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data,
                               void *user_data) {
    BaseType_t high_task_awoken = pdFALSE;
#if CONFIG_DISPLAY_AVOID_TEAR_EFFECT_WITH_SEM
    if (xSemaphoreTakeFromISR(_instance->sem_gui_ready, &high_task_awoken) == pdTRUE) {
        xSemaphoreGiveFromISR(_instance->sem_vsync_end, &high_task_awoken);
    }
#endif
    return high_task_awoken == pdTRUE;
}

void ESP_Panel::lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
#if CONFIG_DISPLAY_AVOID_TEAR_EFFECT_WITH_SEM
    xSemaphoreGive(_instance->sem_gui_ready);
    xSemaphoreTake(_instance->sem_vsync_end, portMAX_DELAY);
#endif
    // pass the draw buffer to the driver
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

void ESP_Panel::increase_lvgl_tick(void *arg) {
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

lv_disp_t *ESP_Panel::begin(bool silent) {
    static lv_disp_draw_buf_t disp_buf;  // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;       // contains callback functions

#if CONFIG_DISPLAY_AVOID_TEAR_EFFECT_WITH_SEM
    ESP_LOGI(TAG, "Create semaphores");
    sem_vsync_end = xSemaphoreCreateBinary();
    assert(sem_vsync_end);
    sem_gui_ready = xSemaphoreCreateBinary();
    assert(sem_gui_ready);
#endif

#if ESP_PANEL_USE_BL
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {.mode = GPIO_MODE_OUTPUT, .pin_bit_mask = 1ULL << ESP_PANEL_LCD_IO_BL};
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
#endif

    ESP_LOGI(TAG, "Install RGB LCD panel driver");
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings =
            {
                .pclk_hz = ESP_PANEL_LCD_RGB_CLK_HZ,
                .h_res = ESP_PANEL_LCD_H_RES,
                .v_res = ESP_PANEL_LCD_V_RES,
                // The following parameters should refer to LCD spec
                .hsync_pulse_width = ESP_PANEL_LCD_RGB_HPW,
                .hsync_back_porch = ESP_PANEL_LCD_RGB_HBP,
                .hsync_front_porch = ESP_PANEL_LCD_RGB_HFP,
                .vsync_pulse_width = ESP_PANEL_LCD_RGB_VPW,
                .vsync_back_porch = ESP_PANEL_LCD_RGB_VBP,
                .vsync_front_porch = ESP_PANEL_LCD_RGB_VFP,
                .flags =
                    {
                        .pclk_active_neg = ESP_PANEL_LCD_RGB_PCLK_ACTIVE_NEG,
                    },

            },
        .data_width = ESP_PANEL_LCD_RGB_DATA_WIDTH,
        .num_fbs = LCD_NUM_FB,
#if CONFIG_DISPLAY_USE_BOUNCE_BUFFER
        .bounce_buffer_size_px = 10 * ESP_PANEL_LCD_H_RES,
#endif
        .psram_trans_align = 64,
        .hsync_gpio_num = ESP_PANEL_LCD_RGB_IO_HSYNC,
        .vsync_gpio_num = ESP_PANEL_LCD_RGB_IO_VSYNC,
        .de_gpio_num = ESP_PANEL_LCD_RGB_IO_DE,
        .pclk_gpio_num = ESP_PANEL_LCD_RGB_IO_PCLK,
        .disp_gpio_num = ESP_PANEL_LCD_RGB_IO_DISP,
        .data_gpio_nums =
            {
                ESP_PANEL_LCD_RGB_IO_DATA0,
                ESP_PANEL_LCD_RGB_IO_DATA1,
                ESP_PANEL_LCD_RGB_IO_DATA2,
                ESP_PANEL_LCD_RGB_IO_DATA3,
                ESP_PANEL_LCD_RGB_IO_DATA4,
                ESP_PANEL_LCD_RGB_IO_DATA5,
                ESP_PANEL_LCD_RGB_IO_DATA6,
                ESP_PANEL_LCD_RGB_IO_DATA7,
                ESP_PANEL_LCD_RGB_IO_DATA8,
                ESP_PANEL_LCD_RGB_IO_DATA9,
                ESP_PANEL_LCD_RGB_IO_DATA10,
                ESP_PANEL_LCD_RGB_IO_DATA11,
                ESP_PANEL_LCD_RGB_IO_DATA12,
                ESP_PANEL_LCD_RGB_IO_DATA13,
                ESP_PANEL_LCD_RGB_IO_DATA14,
                ESP_PANEL_LCD_RGB_IO_DATA15,
            },
        .flags =
            {
                .fb_in_psram = true,  // allocate frame buffer in PSRAM
            },
    };
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &_panel_handle));

    ESP_LOGI(TAG, "Register event callbacks");
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_vsync = on_vsync_event,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(_panel_handle, &cbs, &disp_drv));

    ESP_LOGI(TAG, "Initialize RGB LCD panel");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(_panel_handle));

#if ESP_PANEL_USE_BL
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(ESP_PANEL_LCD_IO_BL, ESP_PANEL_LCD_BL_ON_LEVEL);
#endif

    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    ch422g_begin();

    auto pins = IO_EXPANDER_TOUCH_PANEL_RESET | IO_EXPANDER_LCD_RESET;
    if (!silent) {
        pins |= IO_EXPANDER_LCD_BACKLIGHT;
        _displayState = DisplayState::On;
    }
    ch422g_write_output(pins);

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;

    ESP_LOGI(TAG, "Initialize I2C");

    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();

    ESP_LOGI(TAG, "Initialize touch IO (I2C)");
    /* Touch IO handle */
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_MASTER_NUM, &tp_io_config, &tp_io_handle));
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = ESP_PANEL_LCD_V_RES,
        .y_max = ESP_PANEL_LCD_H_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .flags =
            {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
    };
    /* Initialize touch */
    ESP_LOGI(TAG, "Initialize touch controller GT911");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &_touch_handle));

    ESP_LOGI(TAG, "Initialize LVGL library");

    lv_log_register_print_cb([](const char *buf) { ESP_LOGW("lvgl", "%s", buf); });

    lv_init();
    void *buf1 = NULL;
#if CONFIG_DISPLAY_DOUBLE_FB
    void *buf2 = NULL;
    ESP_LOGI(TAG, "Use frame buffers as LVGL draw buffers");
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(_panel_handle, 2, &buf1, &buf2));
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, ESP_PANEL_LCD_H_RES * ESP_PANEL_LCD_V_RES);
#else
    size_t buf1size = ESP_PANEL_LCD_H_RES * DISPLAY_BUFFER_LINES * sizeof(lv_color_t);
    ESP_LOGI(TAG, "Allocate separate LVGL draw buffers from PSRAM, available %d DMA capable %d, requesting %d",
             heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_free_size(MALLOC_CAP_DMA), buf1size);
    buf1 = heap_caps_malloc(ESP_PANEL_LCD_H_RES * DISPLAY_BUFFER_LINES * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    // buf2 = heap_caps_malloc(ESP_PANEL_LCD_H_RES * ESP_PANEL_LCD_V_RES * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    // assert(buf2);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, NULL, ESP_PANEL_LCD_H_RES * DISPLAY_BUFFER_LINES);
#endif  // CONFIG_DISPLAY_DOUBLE_FB

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = ESP_PANEL_LCD_H_RES;
    disp_drv.ver_res = ESP_PANEL_LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = _panel_handle;
#if CONFIG_DISPLAY_DOUBLE_FB
    disp_drv.full_refresh =
        true;  // the full_refresh mode can maintain the synchronization between the two frame buffers
#endif
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick",
    };

    static lv_indev_drv_t indev_drv;  // Input device driver (Touch)
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev_drv.read_cb = [](lv_indev_drv_t *drv, lv_indev_data_t *data) {
        ((ESP_Panel *)drv->user_data)->lvgl_touch_cb(drv, data);
    };
    indev_drv.user_data = this;

    lv_indev_drv_register(&indev_drv);

    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, ESP_TIMER_MS(LVGL_TICK_PERIOD_MS)));

    const esp_timer_create_args_t displayOffTimerArgs = {
        .callback = [](void *arg) { ((ESP_Panel *)arg)->displayOff(); },
        .arg = this,
        .name = "displayOffTimer",
    };

#if CONFIG_DISPLAY_AUTO_OFF_MS > 0
    ESP_ERROR_CHECK(esp_timer_create(&displayOffTimerArgs, &_displayOffTimer));
    ESP_ERROR_CHECK(esp_timer_start_once(_displayOffTimer, ESP_TIMER_MS(CONFIG_DISPLAY_AUTO_OFF_MS)));
    ESP_LOGI(TAG, "Starting off timer");
#endif

    return disp;
}

void ESP_Panel::process() {
    handleDisplayState();

    // raise the task priority of LVGL and/or reduce the handler period can improve the
    // performance

    vTaskDelay(pdMS_TO_TICKS(10));

    // The task running lv_timer_handler should have lower priority than that running `lv_tick_inc`
    lv_timer_handler();
}

void ESP_Panel::handleDisplayState() {
    // Refuse to turn on the backlight within BACKLIGHT_CHANGE_DEBOUNCE milliseconds.
    // This is to mitigate brownouts.

    auto millis = esp_get_millis();
    if (_lastBacklightChange && (millis - _lastBacklightChange) < BACKLIGHT_CHANGE_DEBOUNCE) {
        return;
    }

    switch (_displayState) {
        case DisplayState::PendingOn:
        case DisplayState::PendingOnFromTouch:
            ESP_LOGI(TAG, "Turning display on");

            _displayState =
                _displayState == DisplayState::PendingOnFromTouch ? DisplayState::OnPendingRelease : DisplayState::On;

#if CONFIG_DISPLAY_DOUBLE_FB
            // We have screen drift problems in double buffering mode. See
            // https://espressif-docs.readthedocs-hosted.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html#why-do-i-get-drift-overall-drift-of-the-display-when-driving-an-rgb-lcd-screen.
            // This resets the panel every time we turn it on, preventing screen
            // drift from becoming permanent.

            esp_lcd_rgb_panel_restart(_panel_handle);
#endif

            _lastBacklightChange = millis;

            _i2cMutex.with([]() {
                ch422g_write_output(IO_EXPANDER_TOUCH_PANEL_RESET | IO_EXPANDER_LCD_BACKLIGHT | IO_EXPANDER_LCD_RESET);
            });
            break;

        case DisplayState::PendingOff:
            ESP_LOGI(TAG, "Turning display off");

            _displayState = DisplayState::Off;

            _lastBacklightChange = millis;

            _i2cMutex.with([]() { ch422g_write_output(IO_EXPANDER_TOUCH_PANEL_RESET | IO_EXPANDER_LCD_RESET); });
            break;
    }
}

void ESP_Panel::displayOn() {
    if (_displayState == DisplayState::Off) {
        _displayState = DisplayState::PendingOn;
    }

    resetDisplayOffTimer();
}

void ESP_Panel::resetDisplayOffTimer() {
#if CONFIG_DISPLAY_AUTO_OFF_MS > 0
    auto ret = esp_timer_restart(_displayOffTimer, CONFIG_DISPLAY_AUTO_OFF_MS * 1000);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(esp_timer_start_once(_displayOffTimer, CONFIG_DISPLAY_AUTO_OFF_MS * 1000));
    } else {
        ESP_ERROR_CHECK(ret);
    }
#endif
}

void ESP_Panel::displayOff() { _displayState = DisplayState::PendingOff; }
