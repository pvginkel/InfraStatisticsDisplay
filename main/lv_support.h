#pragma once

void lv_label_get_text_size(lv_point_t* size_res, const lv_obj_t* obj, int32_t letter_space, int32_t line_space,
                            int32_t max_width, lv_text_flag_t flag);
void lv_write_color(uint8_t** p, uint32_t x, uint32_t y, lv_color32_t color);
void lv_image_create_radial_dsc(lv_img_dsc_t* image_dsc, lv_color32_t start_color, lv_color32_t end_color);
lv_color32_t lv_color32_mix(lv_color32_t c1, lv_color32_t c2, uint8_t mix);
lv_color32_t lv_color32_make(uint8_t red, uint8_t green, uint8_t blue);
void lv_obj_set_grid_cell(lv_obj_t* obj, lv_grid_align_t x_align, uint8_t col_pos, lv_grid_align_t y_align, uint8_t row_pos);
void lv_obj_set_bounds(lv_obj_t* obj, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height, lv_text_align_t align);
