/**
 * Copyright (c) 2026 caszuu
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <hardware/pio.h>
#include <stdint.h>

#include "proto.h"

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
