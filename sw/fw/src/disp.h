/**
 * Copyright (c) 2026 caszuu
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <hardware/pio.h>
#include <stdint.h>

typedef uint8_t disp_flags_t;
enum disp_flag_bits {
    DISP_FLAG_DISABLE_GAMMA_CORRECTION = 0x1,
    DISP_FLAG_DISABLE_STANDBY = 0x2,
    DISP_FLAG_ENTER_SLEEP = 0x4,
};

typedef uint8_t disp_format_t;
enum disp_format {
    DISP_FORMAT_INVALID = 0,

    DISP_FORMAT_RGB565, // (16 bpp)
    DISP_FORMAT_RGB888, // (24 bpp) - also known as rgb24
    DISP_FORMAT_NV12,   // (12 bpp)
};

static const uint32_t mode_magic = 0xceda2083;
struct disp_mode {
    disp_format_t pixel_format;
    disp_flags_t flags;

    uint16_t width, height;
    uint32_t magic_check;
};

/*
 * initialize the display driver hardware. must be called before disp_start()
 */

void disp_init(PIO pio, uint32_t sm_data, uint32_t sm_row);

/*
 * start the display driver loop on the second core
 */

void disp_start();

/*
 * schedule a swap of the current framebuffer being outputed onto the hub75 bus directly
 * before the next frame (to avoid screen tearing).
 *
 * a flip can optionally also change the display config (mode). both will be safely applied
 * on the same frame.
 *
 * note: the in-use framebuffer being replaced will still be used by the display driver
 *       until the next frame begins and it should not become invalid before that point.
 */

void disp_flip(const void *fb, const struct disp_mode *mode);
