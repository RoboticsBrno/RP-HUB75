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

struct nv12_buf {
  uint8_t y_plane[64][64];
  uint8_t uv_plane[32][32][2];
};

// uint8_t fbs[2][64][64][2];
// uint8_t fbs[2][64][64][3];
struct nv12_buf fbs[2];

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

static void read_frame(uint32_t slot_idx) {
  const uint32_t fb_size = sizeof(fbs[0]);
  read_blocking(&fbs[slot_idx], fb_size);
}

int main() {
  stdio_init_all();

  disp_init(pio0, 0, 1);
  disp_start();

  uint32_t frame_idx = 0;
  while (1) {
    read_frame(frame_idx % 2);

    printf("frame_idx: %d present_len: %d us\n", frame_idx, present_len);

    void *fb = &fbs[frame_idx % 2];
    disp_flip(fb, NULL);

    frame_idx++;
  }
}
