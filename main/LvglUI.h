#pragma once

extern "C" {
// Use the script in the tools folder to update the fonts.

LV_FONT_DECLARE(lv_font_roboto_12);
LV_FONT_DECLARE(lv_font_roboto_18);
LV_FONT_DECLARE(lv_font_roboto_24);
LV_FONT_DECLARE(lv_font_roboto_28);
LV_FONT_DECLARE(lv_font_roboto_40);
LV_FONT_DECLARE(lv_font_roboto_100_digits);
}

static constexpr auto XSMALL_FONT = &lv_font_roboto_24;
static constexpr auto SMALL_FONT = &lv_font_roboto_28;
static constexpr auto NORMAL_FONT = &lv_font_roboto_40;
static constexpr auto LARGE_DIGITS_FONT = &lv_font_roboto_100_digits;

class LvglUI {
    vector<lv_obj_t*> _loading_circles;

public:
    LvglUI() {}
    LvglUI(const LvglUI& other) = delete;
    LvglUI(LvglUI&& other) noexcept = delete;
    LvglUI& operator=(const LvglUI& other) = delete;
    LvglUI& operator=(LvglUI&& other) noexcept = delete;
    virtual ~LvglUI();

    void begin();
    void render();
    void update();

protected:
    virtual void do_render(lv_obj_t* parent) = 0;
    virtual void do_begin() {}
    virtual void do_update() {}

    lv_coord_t pw(double value) const { return lv_coord_t(LV_HOR_RES * (value / 100)); }
    lv_coord_t ph(double value) const { return lv_coord_t(LV_VER_RES * (value / 100)); }
    void render_loading_ui(lv_obj_t* parent);
    static void loading_animation_callback(void* var, int32_t v);
    void remove_loading_ui();
    void reset_outer_container_styles(lv_obj_t* cont);
    void reset_layout_container_styles(lv_obj_t* cont);
};
