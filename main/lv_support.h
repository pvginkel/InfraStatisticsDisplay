#pragma once

void lv_label_get_text_size(lv_point_t* size_res, const lv_obj_t* obj, int32_t letter_space, int32_t line_space,
                            int32_t max_width, lv_text_flag_t flag);
void lv_obj_set_grid_cell(lv_obj_t* obj, lv_grid_align_t x_align, uint8_t col_pos, lv_grid_align_t y_align,
                          uint8_t row_pos);
void lv_obj_set_bounds(lv_obj_t* obj, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height,
                       lv_text_align_t align);
