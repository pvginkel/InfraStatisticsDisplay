#pragma once

#ifndef LV_SIMULATOR

#include "waveshare_epaper.h"

class Device {
public:
    Device() {}

    bool begin();
    void process();

private:
    WaveshareEPaper7P5InV2alt _display;

    void flush_cb(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p);
};

#endif
