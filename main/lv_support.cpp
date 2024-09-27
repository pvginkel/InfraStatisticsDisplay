#include "includes.h"

#include "lv_support.h"

void lv_label_get_text_size(lv_point_t* size_res, const lv_obj_t* obj, int32_t letter_space, int32_t line_space,
                            int32_t max_width, lv_text_flag_t flag) {
    const auto text = lv_label_get_text(obj);
    const auto font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);

    lv_txt_get_size(size_res, text, font, letter_space, line_space, max_width, flag);
}

void lv_obj_set_grid_cell(lv_obj_t* obj, lv_grid_align_t x_align, uint8_t col_pos, lv_grid_align_t y_align,
                          uint8_t row_pos) {
    lv_obj_set_grid_cell(obj, x_align, col_pos, 1, y_align, row_pos, 1);
}

void lv_obj_set_bounds(lv_obj_t* obj, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height,
                       lv_text_align_t align) {
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
