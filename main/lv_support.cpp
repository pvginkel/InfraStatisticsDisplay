#include "includes.h"

#include "lv_support.h"

static float fast_rsqrt(float x) {
    float xhalf = 0.5f * x;
    int i = *(int*)&x;
    i = 0x5f3759df - (i >> 1);
    x = *(float*)&i;
    x = x * (1.5f - xhalf * x * x);
    return x;
}

static float fast_sqrt(float x) { return fast_rsqrt(x) * x; }

void lv_label_get_text_size(lv_point_t* size_res, const lv_obj_t* obj, int32_t letter_space, int32_t line_space,
                            int32_t max_width, lv_text_flag_t flag) {
    const auto text = lv_label_get_text(obj);
    const auto font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);

    lv_txt_get_size(size_res, text, font, letter_space, line_space, max_width, flag);
}

void lv_image_create_radial_dsc(lv_img_dsc_t* image_dsc, lv_color32_t start_color, lv_color32_t end_color) {
    auto width = image_dsc->header.w;
    auto height = image_dsc->header.h;

    image_dsc->data_size = width * height * (LV_COLOR_DEPTH / 8);
#ifdef LV_SIMULATOR
    image_dsc->data = (uint8_t*)malloc(image_dsc->data_size);
#else
    image_dsc->data = (uint8_t*)heap_caps_malloc(image_dsc->data_size, MALLOC_CAP_SPIRAM);
#endif
    image_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;

    auto p = (uint8_t*)image_dsc->data;
    int cx = width / 2;
    int cy = height / 2;
    int min = cx < cy ? cx : cy;

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            auto dx = abs(cx - int(x));
            auto dy = abs(cy - int(y));
            auto distance = fast_sqrt(dx * dx + dy * dy);
            auto ratio = distance / min;
            auto mix = ratio < 0 ? 0 : ratio > 1 ? 255 : (int)(ratio * 255);

            lv_write_color(&p, x, y, lv_color32_mix(start_color, end_color, mix));
        }
    }
}

lv_color32_t lv_color32_mix(lv_color32_t c1, lv_color32_t c2, uint8_t mix) {
    lv_color32_t ret;

    ret.ch.red = LV_UDIV255((uint16_t)c1.ch.red * mix + (uint16_t)c2.ch.red * (255 - mix) + LV_COLOR_MIX_ROUND_OFS);
    ret.ch.green =
        LV_UDIV255((uint16_t)c1.ch.green * mix + (uint16_t)c2.ch.green * (255 - mix) + LV_COLOR_MIX_ROUND_OFS);
    ret.ch.blue = LV_UDIV255((uint16_t)c1.ch.blue * mix + (uint16_t)c2.ch.blue * (255 - mix) + LV_COLOR_MIX_ROUND_OFS);
    ret.ch.alpha = 0xff;

    return ret;
}

void lv_write_color(uint8_t** p, uint32_t x, uint32_t y, lv_color32_t color) {
#if LV_COLOR_DEPTH == 32
    *((uint32_t*)(*p)) = lv_color_to32(color);
#elif LV_COLOR_DEPTH == 16
    *((uint16_t*)(*p)) = dither_color_xy(x, y, color);
#else
#error Unsupported
#endif

    (*p) += LV_COLOR_DEPTH / 8;
}

lv_color32_t lv_color32_make(uint8_t red, uint8_t green, uint8_t blue) {
    lv_color32_t ret;

    ret.ch.red = red;
    ret.ch.green = green;
    ret.ch.blue = blue;
    ret.ch.alpha = 0xff;

    return ret;
}

void lv_obj_set_grid_cell(lv_obj_t* obj, lv_grid_align_t x_align, uint8_t col_pos, lv_grid_align_t y_align, uint8_t row_pos) {
    lv_obj_set_grid_cell(obj, x_align, col_pos, 1, y_align, row_pos, 1);
}

void lv_obj_set_bounds(lv_obj_t* obj, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height, lv_text_align_t align) {
    lv_obj_set_size(obj, width, height);

    switch (align) {
    case LV_TEXT_ALIGN_LEFT:
        lv_obj_set_x(obj, x);
        break;

    case LV_TEXT_ALIGN_CENTER:
        lv_obj_set_x(obj, x - width / 2);
        break;

    case LV_TEXT_ALIGN_RIGHT:
        lv_obj_set_x(obj, x - width);
        break;
    }

    lv_obj_set_y(obj, y - height / 2);
}
