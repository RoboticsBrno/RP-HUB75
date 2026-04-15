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

static const uint16_t min_sync_steps = 2;

static uint32_t scan_for_sync() {
    while (1) {
        uint32_t in_sync_steps = 0;
        uint16_t lfsr;

        // read-in initial lfsr state
        disp_recv_blocking(&lfsr, 2); // TODO: standby on timeout

        while (1) {
            // shift the lfsr by one step
            lfsr ^= lfsr >> 7;
            lfsr ^= lfsr << 9;
            lfsr ^= lfsr >> 13;

            // check againts the incomming stream
            uint16_t target;
            disp_recv_blocking(&target, 2);

            if (lfsr != target)
                break; // lfsr chain broken, discard sync

            if (in_sync_steps++ < min_sync_steps)
                continue; // minimal number of lfsr steps not reached, wait for next sync step

            if (lfsr == disp_sync_symbol)
                return disp_sync_symbol;
        }

        // scan timeout: discard some bytes to catch up; offset by odd amount of bytes to test the second 16-bit alignment
        uint8_t discard[4];
        disp_recv_blocking(discard, 3);

        // FIXME: also implement a qspi reset trigger
        printf("sync failed: %d\n", in_sync_steps);
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
