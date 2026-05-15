/**
 * Copyright (c) 2026 caszuu
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <stdint.h>

/*
 * Init the input link.
 */

void disp_recv_init();

/*
 * read a set amount of bytes from the input link.
 * returns the number of bytes read on success or -1 on error / timeout.
 */

int disp_recv_blocking(void *buf, uint32_t size);
