#include "includes.h"

#ifndef LV_SIMULATOR

#include "Device.h"
#include "lvgl.h"

#define LVGL_TICK_PERIOD_MS 2

LOG_TAG(Device);

void Device::process() {
    // raise the task priority of LVGL and/or reduce the handler period can improve the
    // performance

    vTaskDelay(pdMS_TO_TICKS(10));

    // The task running lv_timer_handler should have lower priority than that running `lv_tick_inc`
    lv_timer_handler();
}

void Device::flush_cb(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    ESP_LOGI(TAG, "Updating display");

    auto target = _display.get_buffer();

    // Ensure width is a multiple of 8.
    const auto width = _display.get_width();
    const auto scanline_width = (width + 7) & ~7;
    const auto height = _display.get_height();

    uint8_t byte = 0;

    for (auto y = 0; y < height; y++) {
        for (auto x = 0; x < scanline_width; x++) {
            auto pixel = color_p[y * width + x].full;

            byte |= pixel << (7 - x % 8);

            if (x % 8 == 7) {
                *(target++) = byte;
                byte = 0;
            }
        }
    }

    _display.update();

    ESP_LOGI(TAG, "Finished updating display");

    lv_disp_flush_ready(disp_drv);
}

bool Device::begin() {
    gpio_reset_pin((gpio_num_t)EPD_PWR_PIN);
    gpio_set_direction((gpio_num_t)EPD_PWR_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)EPD_PWR_PIN, 1);

    _display.set_busy_pin(new GPIOPin(EPD_BUSY_PIN, GPIO_MODE_INPUT, true /* inverted */));
    _display.set_cs_pin(new GPIOPin(EPD_CS_PIN, GPIO_MODE_OUTPUT));
    _display.set_dc_pin(new GPIOPin(EPD_DC_PIN, GPIO_MODE_OUTPUT));
    _display.set_reset_pin(new GPIOPin(EPD_RST_PIN, GPIO_MODE_OUTPUT));
    _display.set_full_update_every(1);
    _display.setup();

    lv_init();

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = [](void* arg) { lv_tick_inc(LVGL_TICK_PERIOD_MS); },
        .name = "lvgl_tick",
    };

    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, ESP_TIMER_MS(LVGL_TICK_PERIOD_MS)));

    ESP_LOGI(TAG, "Allocating %d Kb for draw buffer",
             (sizeof(lv_color_t) * _display.get_width() * _display.get_height()) / 1024);

    static lv_disp_draw_buf_t draw_buffer_dsc;
    auto draw_buffer = (lv_color_t*)heap_caps_malloc(sizeof(lv_color_t) * _display.get_width() * _display.get_height(),
                                                     MALLOC_CAP_SPIRAM);
    if (!draw_buffer) {
        ESP_LOGE(TAG, "Failed to allocate draw buffer");
        esp_restart();
    }

    lv_disp_draw_buf_init(&draw_buffer_dsc, draw_buffer, nullptr, _display.get_width() * _display.get_height());

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res = _display.get_width();
    disp_drv.ver_res = _display.get_height();

    disp_drv.flush_cb = [](lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
        ((Device*)disp_drv->user_data)->flush_cb(disp_drv, area, color_p);
    };
    disp_drv.user_data = this;

    disp_drv.draw_buf = &draw_buffer_dsc;

    disp_drv.full_refresh = 1;
    disp_drv.dpi = LV_DPI_DEF;

    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Device initialization complete");

    return true;
}

#endif
