#include "includes.h"

#include "LvglUI.h"

void lv_obj_set_bounds(lv_obj_t* obj, int x, int y, int width, int height, lv_text_align_t align) {
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

LvglUI::LvglUI(lv_obj_t* parent) : _parent(parent), _screenWidth(), _screenHeight() {}

void LvglUI::begin() {
    _screenWidth = LV_HOR_RES;
    _screenHeight = LV_VER_RES;

    doBegin();
}

void LvglUI::render() {
    lv_obj_clean(_parent);

    lv_theme_default_init(nullptr, lv_palette_main(LV_PALETTE_GREY), lv_palette_main(LV_PALETTE_GREY),
                          LV_THEME_DEFAULT_DARK, LV_FONT_DEFAULT);

    lv_obj_set_style_bg_color(_parent, lv_color_black(), LV_PART_MAIN);

    doRender(_parent);
}
