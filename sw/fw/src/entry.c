/**
 * Copyright (c) 2026 caszuu
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "disp.h"

#include <pico/error.h>
#include <pico/stdio.h>
#include <pico/time.h>

#include <pico/types.h>
#include <stdio.h>

union fb_buf {
    uint8_t rgb565[64][64][2];
    uint8_t rgb888[64][64][3];

    struct nv12_buf {
        uint8_t y_plane[64][64];
        uint8_t uv_plane[32][32][2];
    } nv12;
};

union fb_buf fbs[2];
struct disp_mode mode;

extern uint32_t present_len;

static void read_blocking(void *dst, uint32_t n_bytes) {
    uint32_t to_read = n_bytes;
    uint8_t *buf = (uint8_t *)dst;

    absolute_time_t timeout = get_absolute_time() + 100;

    while (to_read) {
        int n = stdio_get_until((char *)buf, to_read, timeout);
        if (n == PICO_ERROR_TIMEOUT)
            continue;

        buf += n;
        to_read -= n;
    }
}

static const uint16_t sync_seq[8] = {
    0xac92,
    0x3bca,
    0x41bf,
    0x393d,
    0xa74a,
    0xae01,
    0x155d,
    0xfb70,
};

static const uint16_t sync_symbol = 0xfb70; // last state from lfsr.py
static const uint16_t min_sync_steps = 2;

static uint32_t scan_for_sync() {
    absolute_time_t timeout = get_absolute_time() + 100;

    while (1) {
        uint32_t in_sync_steps = 0;
        uint16_t lfsr;

        // read-in initial lfsr state
        read_blocking(&lfsr, 2); // TODO: standby on timeout

        while (1) {
            // shift the lfsr by one step
            lfsr ^= lfsr >> 7;
            lfsr ^= lfsr << 9;
            lfsr ^= lfsr >> 13;

            // check againts the incomming stream
            uint16_t target;
            read_blocking(&target, 2);

            if (lfsr != target)
                break; // lfsr chain broken, discard sync

            if (in_sync_steps++ < min_sync_steps)
                continue; // minimal number of lfsr steps not reached, wait for next sync step

            if (lfsr == sync_symbol)
                return sync_symbol;
        }

        // scan timeout: discard some bytes to catch up; offset by odd amount of bytes to test the second 16-bit alignment
        uint8_t discard[4];
        read_blocking(discard, 3);

        // FIXME: also implement a qspi reset trigger
        printf("sync failed: %d\n", in_sync_steps);
    }

    return 0;
}

static void read_modeset() {
    struct disp_mode m;
    read_blocking(&m, sizeof(m));

    if (m.magic_check != mode_magic)
        return;

    mode = m;
}

static void read_frame(uint32_t slot_idx) {
    uint32_t fb_size;

    switch (mode.pixel_format) {
    case DISP_FORMAT_RGB565:
        fb_size = sizeof(fbs->rgb565);
        break;

    case DISP_FORMAT_RGB888:
        fb_size = sizeof(fbs->rgb888);
        break;

    case DISP_FORMAT_NV12:
        fb_size = sizeof(fbs->nv12);
        break;

    default:
        fb_size = 0;
        break;
    }

    read_blocking(&fbs[slot_idx], fb_size);
}

int main() {
    disp_init(pio0, 0, 1);
    disp_start();

    stdio_init_all();

    mode = (struct disp_mode){
        .flags = 0,
        .width = 64,
        .height = 64,
        .pixel_format = DISP_FORMAT_NV12,
        .magic_check = mode_magic,
    };
    disp_flip(NULL, &mode);

    uint32_t frame_idx = 0;
    while (1) {

#if 0
        read_frame(frame_idx % 2);
#else
        uint16_t symbol = scan_for_sync();
        if (symbol == 0)
            continue;

        // read_modeset();
        read_frame(frame_idx % 2);
#endif

        printf("frame_idx: %d present_len: %d us\n", frame_idx, present_len);

        void *fb = &fbs[frame_idx % 2];
        disp_flip(fb, NULL);

        frame_idx++;
    }
}
