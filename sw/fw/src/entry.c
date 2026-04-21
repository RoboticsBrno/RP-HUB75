/**
 * Copyright (c) 2026 caszuu
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "disp.h"
#include "proto.h"
#include "recv.h"

#include <pico/stdio.h>
#include <stdio.h>

static uint8_t fbs[2][DISP_MAX_FRAME_SIZE];
static struct disp_mode mode;

extern uint32_t present_len;

// stream parser //

static const uint16_t min_sync_steps = 4;

static uint32_t scan_for_sync() {
    uint32_t in_sync_steps = 0;

    while (1) {
        uint16_t buf[2];

        // read-in initial lfsr state
        disp_recv_blocking(buf, 4); // TODO: standby on timeout

        uint16_t lfsr = buf[0], target = buf[1];

        // shift the lfsr by one step
        lfsr ^= lfsr >> 7;
        lfsr ^= lfsr << 9;
        lfsr ^= lfsr >> 13;

        if (lfsr != target) {
            // sync seq broken, discard some bytes to catch up
            uint8_t discard[12];
            disp_recv_blocking(discard, 12);

            // printf("sync failed: %d\n", in_sync_steps);

            in_sync_steps = 0;
            continue;
        }

        if (in_sync_steps++ < min_sync_steps)
            continue; // minimal number of lfsr steps not reached, wait for next sync step

        if (lfsr == disp_sync_symbol)
            return disp_sync_symbol;
    }

    return 0;
}

static void read_modeset() {
    struct disp_mode m;
    disp_recv_blocking(&m, sizeof(m));

    if (m.magic != disp_mode_magic_value)
        return;

    mode = m;
}

static void read_frame(uint32_t slot_idx) {
    uint32_t fb_size;

    switch (mode.format) {
    case DISP_FORMAT_RGB565:
        fb_size = mode.width * 64 * 2;
        break;

    case DISP_FORMAT_RGB888:
        fb_size = mode.width * 64 * 3;
        break;

    case DISP_FORMAT_NV12:
        fb_size = mode.width * 64 + mode.width * 64 / 4 * 2;
        break;

    default:
        fb_size = 0;
        break;
    }

    if (fb_size > DISP_MAX_FRAME_SIZE)
        return; // read would buffer overrun, ignore frame

    disp_recv_blocking(&fbs[slot_idx], fb_size);
}

int main() {
    disp_init(pio0, 0, 1);
    disp_start();

    stdio_init_all();
    disp_recv_init();

    mode = (struct disp_mode){};
    disp_flip(NULL, &mode);

    uint32_t frame_idx = 0;
    while (1) {
        uint16_t symbol = scan_for_sync();
        if (symbol == 0)
            continue;

        read_modeset();
        read_frame(frame_idx % 2);

        printf("frame_idx: %d present_len: %d us\n", frame_idx, present_len);

        void *fb = &fbs[frame_idx % 2];
        disp_flip(fb, &mode);

        frame_idx++;
    }
}
