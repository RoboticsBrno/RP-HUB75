/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * Copyright (c) 2026 caszuu
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "disp.h"

#include "hardware/pio.h"
#include "hub75.pio.h"
#include "pico/multicore.h"

#include <assert.h>
#include <hardware/timer.h>
#include <stdio.h>

#define DATA_BASE_PIN 0
#define DATA_N_PINS 6
#define ROWSEL_BASE_PIN 6
#define ROWSEL_N_PINS 5
#define CLK_PIN 11
#define STROBE_PIN 12
#define OEN_PIN 13

#define MAX_WIDTH 128

struct disp_state {
    PIO pio;

    uint32_t sm_data;
    uint32_t sm_row;
    uint32_t data_prog_offs;

    const uint8_t *fb;
    struct disp_mode mode;

    // note: accessed by both cores, must be volatile
    const uint8_t *volatile next_fb;
    struct disp_mode volatile next_mode;
};

static struct disp_state disp;

// performance timers //
uint32_t present_len;

// pixel processing //

static inline uint32_t gamma_correct_565(uint16_t pix) {
    uint32_t r_gamma = pix & 0xf800u;
    r_gamma *= r_gamma;
    uint32_t g_gamma = pix & 0x07e0u;
    g_gamma *= g_gamma;
    uint32_t b_gamma = pix & 0x001fu;
    b_gamma *= b_gamma;
    return (b_gamma >> 2 << 16) | (g_gamma >> 14 << 8) | (r_gamma >> 24 << 0);
}

static inline uint32_t gamma_correct_888(uint32_t pix) {
    uint64_t r_gamma = pix & 0x000000ff;
    r_gamma *= r_gamma;
    uint64_t g_gamma = pix & 0x0000ff00;
    g_gamma *= g_gamma;
    uint64_t b_gamma = pix & 0x00ff0000;
    b_gamma *= b_gamma;
    return (b_gamma >> 40 << 16) | (g_gamma >> 24 << 8) | (r_gamma >> 8 << 0);
}

static inline uint32_t gamma_correct_nv12(uint32_t pix_x, uint32_t pix_y) {
    // fetch planar pixel components
    const uint32_t y_plane_size = disp.mode.width * disp.mode.height;
    int16_t y = disp.fb[pix_x + disp.mode.width * pix_y];

    uint32_t uv_pix_idx = pix_x / 2 + (disp.mode.width / 2) * (pix_y / 2);

    int16_t u = disp.fb[y_plane_size + uv_pix_idx * 2 + 0] - 128;
    int16_t v = disp.fb[y_plane_size + uv_pix_idx * 2 + 1] - 128;

    // fixed-point conversion (broken)
    // uint8_t r = y + ((357 * v) >> 8);
    // uint8_t g = y - ((87 * u) >> 8) - ((182 * v) >> 8);
    // uint8_t b = y + ((451 * u) >> 8);

    // floating-point conversion (slower, but working)
    uint8_t r = y + (1.403f * v);
    uint8_t g = y - (0.344f * u) - (0.714f * v);
    uint8_t b = y + (1.770f * u);

    // gamma correction
    uint64_t r_gamma = r;
    r_gamma *= r_gamma;
    uint64_t g_gamma = g;
    g_gamma *= g_gamma;
    uint64_t b_gamma = b;
    b_gamma *= b_gamma;

    return (b_gamma >> 8 << 16) | (g_gamma >> 8 << 8) | (r_gamma >> 8 << 0);
}

// refresh loops //

/*
 * accept an interleaved row pair buffer (RGBX0r0, RGBX0r1, RGBX1r0, RGBX1r1) and shift the out to the hub75 bus.
 */

static inline void shift_and_latch_row(uint32_t row_idx, uint32_t inter_row[MAX_WIDTH][2]) {
    for (int bit = 0; bit < 8; ++bit) {
        hub75_data_rgb888_set_shift(disp.pio, disp.sm_data, disp.data_prog_offs, bit);
        for (int x = 0; x < disp.mode.width; ++x) {
            pio_sm_put_blocking(disp.pio, disp.sm_data, inter_row[x][0]);
            pio_sm_put_blocking(disp.pio, disp.sm_data, inter_row[x][1]);
        }

        // Dummy pixel per lane
        pio_sm_put_blocking(disp.pio, disp.sm_data, 0);
        pio_sm_put_blocking(disp.pio, disp.sm_data, 0);

        // SM is finished when it stalls on empty TX FIFO
        hub75_wait_tx_stall(disp.pio, disp.sm_data);

        // Also check that previous OEn pulse is finished, else things can get
        // out of sequence
        hub75_wait_tx_stall(disp.pio, disp.sm_row);

        // Mangle rowsel bits into the RP-HUB75 layout (EABCD)
        int rowsel = (row_idx << 1) | (row_idx >> (ROWSEL_N_PINS - 1)) & ((1 << ROWSEL_N_PINS) - 1);

        // Latch row data, pulse output enable for new row.
        pio_sm_put_blocking(disp.pio, disp.sm_row, rowsel | (50u * (1u << bit) << 5));
    }
}

static uint32_t gc_rows[MAX_WIDTH][2];

static void refresh_display_565() {
    for (int row_idx = 0; row_idx < (1 << ROWSEL_N_PINS); ++row_idx) {
        for (int x = 0; x < disp.mode.width; ++x) {
            gc_rows[x][0] = gamma_correct_565(*(uint16_t *)&disp.fb[(row_idx * disp.mode.width + x) * 2]);
            gc_rows[x][1] = gamma_correct_565(*(uint16_t *)&disp.fb[(((1u << ROWSEL_N_PINS) + row_idx) * disp.mode.width + x) * 2]);
        }

        shift_and_latch_row(row_idx, gc_rows);
    }
}

static void refresh_display_888() {
    for (int row_idx = 0; row_idx < (1 << ROWSEL_N_PINS); ++row_idx) {
        for (int x = 0; x < disp.mode.width; ++x) {
            // TODO: unaligned reads?
            gc_rows[x][0] = gamma_correct_888(*(uint32_t *)&disp.fb[(row_idx * disp.mode.width + x) * 3]);
            gc_rows[x][1] = gamma_correct_888(*(uint32_t *)&disp.fb[(((1u << ROWSEL_N_PINS) + row_idx) * disp.mode.width + x) * 3]);
        }

        shift_and_latch_row(row_idx, gc_rows);
    }
}

static void refresh_display_nv12() {
    for (int row_idx = 0; row_idx < (1 << ROWSEL_N_PINS); ++row_idx) {
        for (int x = 0; x < disp.mode.width; ++x) {
            gc_rows[x][0] = gamma_correct_nv12(x, row_idx);
            gc_rows[x][1] = gamma_correct_nv12(x, ((1u << ROWSEL_N_PINS) + row_idx));
        }

        shift_and_latch_row(row_idx, gc_rows);
    }
}

void disp_init(PIO pio, uint32_t sm_data, uint32_t sm_row) {
    disp = (struct disp_state){
        .pio = pio,
        .sm_data = sm_data,
        .sm_row = sm_row,

        .fb = NULL,
        .mode = {
            .pixel_format = DISP_FORMAT_NV12,
            .flags = 0,

            .width = 64,
            .height = 64,
        },
    };

    uint data_prog_offs = pio_add_program(pio, &hub75_data_rgb888_program);
    uint row_prog_offs = pio_add_program(pio, &hub75_row_program);

    disp.data_prog_offs = data_prog_offs;

    hub75_data_rgb888_program_init(pio, sm_data, data_prog_offs, DATA_BASE_PIN, CLK_PIN);
    hub75_row_program_init(pio, sm_row, row_prog_offs, ROWSEL_BASE_PIN, ROWSEL_N_PINS, STROBE_PIN);
}

static void disp_loop() {
    while (1) {
        // FIXME: frame swap locking

        if (disp.next_fb) {
            disp.fb = disp.next_fb;
            disp.next_fb = NULL;
        }

        if (!disp.fb)
            continue;

        uint64_t t0 = time_us_64();

        switch (disp.mode.pixel_format) {
        case DISP_FORMAT_RGB565:
            refresh_display_565();
            break;

        case DISP_FORMAT_RGB888:
            refresh_display_888();
            break;

        case DISP_FORMAT_NV12:
            refresh_display_nv12();
            break;

        default:
            assert(false && "Unreachable");
        }

        present_len = (uint32_t)(time_us_64() - t0);
    }
}

void disp_start() {
    multicore_launch_core1(disp_loop);
}

void disp_flip(const void *fb, const struct disp_mode *mode) {
    // FIXME: frame swap locking

    disp.next_fb = fb;
    if (mode)
        disp.next_mode = *mode;
}
