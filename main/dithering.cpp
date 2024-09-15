#include "includes.h"

#include "dithering.h"

// Taken from https://stackoverflow.com/questions/11640017.

// clang-format off

/* Dither Tresshold for Red Channel */
static const uint8_t dither_tresshold_r[64] = {
  1, 7, 3, 5, 0, 8, 2, 6,
  7, 1, 5, 3, 8, 0, 6, 2,
  3, 5, 0, 8, 2, 6, 1, 7,
  5, 3, 8, 0, 6, 2, 7, 1,

  0, 8, 2, 6, 1, 7, 3, 5,
  8, 0, 6, 2, 7, 1, 5, 3,
  2, 6, 1, 7, 3, 5, 0, 8,
  6, 2, 7, 1, 5, 3, 8, 0
};

/* Dither Tresshold for Green Channel */
static const uint8_t dither_tresshold_g[64] = {
  1, 3, 2, 2, 3, 1, 2, 2,
  2, 2, 0, 4, 2, 2, 4, 0,
  3, 1, 2, 2, 1, 3, 2, 2,
  2, 2, 4, 0, 2, 2, 0, 4,

  1, 3, 2, 2, 3, 1, 2, 2,
  2, 2, 0, 4, 2, 2, 4, 0,
  3, 1, 2, 2, 1, 3, 2, 2,
  2, 2, 4, 0, 2, 2, 0, 4
};

/* Dither Tresshold for Blue Channel */
static const uint8_t dither_tresshold_b[64] = {
  5, 3, 8, 0, 6, 2, 7, 1,
  3, 5, 0, 8, 2, 6, 1, 7,
  8, 0, 6, 2, 7, 1, 5, 3,
  0, 8, 2, 6, 1, 7, 3, 5,

  6, 2, 7, 1, 5, 3, 8, 0,
  2, 6, 1, 7, 3, 5, 0, 8,
  7, 1, 5, 3, 8, 0, 6, 2,
  1, 7, 3, 5, 0, 8, 2, 6
};

// clang-format on

/* Get 16bit closest color */
static uint8_t closest_rb(uint8_t c) { return (c >> 3 << 3); /* red & blue */ }
static uint8_t closest_g(uint8_t c) { return (c >> 2 << 2); /* green */ }

/* Dithering by individual subpixel */
static uint16_t dither_xy(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    /* Get Tresshold Index */
    auto tresshold_id = ((y & 7) << 3) + (x & 7);

    r = closest_rb(min(r + dither_tresshold_r[tresshold_id], 0xff));
    g = closest_g(min(g + dither_tresshold_g[tresshold_id], 0xff));
    b = closest_rb(min(b + dither_tresshold_b[tresshold_id], 0xff));

    return lv_color_to16(lv_color_make(r, g, b));
}

/* Dithering Pixel from 32/24bit RGB */
uint16_t dither_color_xy(uint32_t x, uint32_t y, lv_color32_t col) {
    return dither_xy(x, y, col.ch.red, col.ch.green, col.ch.blue);
}
