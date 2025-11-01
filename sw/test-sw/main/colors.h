// colors.h
// Small colors helper library that uses the project's `rgb()` function to
// construct common colors.

#ifndef COLORS_H
#define COLORS_H

#include "rphub75.h"

static const rpio_rgb_t color_black   = { .r = 0,   .g = 0,   .b = 0   };
static const rpio_rgb_t color_white   = { .r = 255, .g = 255, .b = 255 };
static const rpio_rgb_t color_red     = { .r = 255, .g = 0,   .b = 0   };
static const rpio_rgb_t color_green   = { .r = 0,   .g = 255, .b = 0   };
static const rpio_rgb_t color_blue    = { .r = 0,   .g = 0,   .b = 255 };
static const rpio_rgb_t color_yellow  = { .r = 255, .g = 255, .b = 0   };
static const rpio_rgb_t color_cyan    = { .r = 0,   .g = 255, .b = 255 };
static const rpio_rgb_t color_magenta = { .r = 255, .g = 0,   .b = 255 };
static const rpio_rgb_t color_orange  = { .r = 255, .g = 165, .b = 0   };
static const rpio_rgb_t color_purple  = { .r = 128, .g = 0,   .b = 128 };
static const rpio_rgb_t color_gray    = { .r = 128, .g = 128, .b = 128 };

#endif // COLORS_H
