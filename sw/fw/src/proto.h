/**
 * Copyright (c) 2026 caszuu
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <stdint.h>

/*
 * The saturn ring (display) protocol.
 *
 * The display listens on a single uni-directional QSPI link as a slave. By default, the display will be in
 * scan mode and it will look for any valid sync sequence. After it is found, the next frame is received and
 * the display returns into scan mode awaiting the next frame.
 *
 * A frame should be structured on the QSPI bus as follows:
 *
 *  [sync sequence] [disp_mode struct] [image buffer]
 *
 * - sync sequence - should always be a direct 16-byte copy of disp_sync_seq
 * - disp_mode struct - should be a valid struct disp_mode with config for this frame
 * - image buffer - a raw row-major array of pixels (or planes) in the selected format
 *                  the size of this buffer must == mode.width * 64 * pixel_size and be less or equal to DISP_MAX_FRAME_SIZE
 *
 * All QSPI transactions (as delimited by CS asserts and deasserts) must always be 4-byte aligned, otherwise data will be lost.
 * TODO: different sync sequences can be used to implement different commands in the future... (eg. palletes)
 */

// sync sequence //

static const uint16_t disp_sync_seq[16] = {0xac92, 0x3bca, 0x41bf, 0x393d, 0xa74a, 0xae01, 0x155d, 0xfb70, 0xf681, 0x2f6d, 0x4931, 0x0fa3, 0x77bf, 0xd756, 0x26f9, 0x4eb6};
static const uint16_t disp_sync_symbol = 0x4eb6; // last state from disp_sync_seq

// display mode //

#define DISP_MAX_FRAME_SIZE 65535

typedef uint8_t disp_flags_t;
enum disp_flag_bits {
    DISP_FLAG_DISABLE_GAMMA_CORRECTION = 0x1,
    // to be extended...
};

// NOTE: only RGB565, RGB888 and NV12 are supported, others are ignored

typedef uint8_t disp_format_t;
enum disp_format {
    DISP_FORMAT_INVALID = 0,

    DISP_FORMAT_1BIT = 3,      // 1-bit monochrome
    DISP_FORMAT_GRAY4 = 4,     // 4-bit grayscale
    DISP_FORMAT_GRAY8 = 5,     // 8-bit grayscale
    DISP_FORMAT_RGB332 = 6,    // RGB 3:3:2
    DISP_FORMAT_RGB565 = 7,    // RGB 5:6:5 (little-endian)
    DISP_FORMAT_RGB565BE = 8,  // RGB 5:6:5 (big-endian)
    DISP_FORMAT_RGB888 = 9,    // RGB 8:8:8
    DISP_FORMAT_RGBX8888 = 10, // RGBX 8:8:8:8
    DISP_FORMAT_XRGB4444 = 12, // XRGB 4:4:4:4

    DISP_FORMAT_NV12 = 13,   // NV12   Y0 Y1 Y2 Y3 | U V
    DISP_FORMAT_YUV422 = 20, // YUV422 Y0 U Y1 V 8:8:8:8
};

static const uint8_t disp_mode_magic_value = 0xfb;

struct disp_mode {
    uint8_t magic;        // must be equal to disp_mode_magic_value, the mode struct is ignored otherwise
    disp_flags_t flags;   // misc display flags; see disp_frag_bits enum for descriptions
    disp_format_t format; // the pixel format of the next incomming frame
    uint8_t brightness;   // display brightness; in range [0-255]

    uint16_t width; // arbitrary, if width is bigger than the display row length, data is output to the next daisy chained display
                    // height is always assumed to equal 64

    uint16_t __padding; // align struct to 4 bytes
};
