/**
 * Copyright (c) 2026 caszuu
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifdef SR_ENABLE_INPUT_USB
#include <pico/error.h>
#include <pico/stdio.h>
#include <pico/time.h>

void disp_recv_init() {
    // stdio_init_all();
}

int disp_recv_blocking(void *ptr, uint32_t size) {
    uint32_t to_read = size;
    char *buf = (char *)ptr;

    absolute_time_t timeout = get_absolute_time() + 100;

    while (to_read) {
        int n = stdio_get_until(buf, to_read, timeout);
        if (n == PICO_ERROR_TIMEOUT)
            continue;

        buf += n;
        to_read -= n;
    }

    return size;
}
#endif // SR_ENABLE_INPUT_USB

#ifdef SR_ENABLE_INPUT_QSPI
#include "qrx.pio.h"
#include <hardware/pio.h>

static PIO qspi_rx_pio;
static uint qspi_rx_sm;

static void qspi_init(PIO pio, uint sm) {
    uint offset = pio_add_program(pio, &qspi_rx_program);
    qspi_rx_program_init(pio, sm, offset, 19, 23);

    qspi_rx_pio = pio;
    qspi_rx_sm = sm;
}

static int qspi_get(uint8_t *buf, uint32_t max_bytes) {
    assert(max_bytes % 4 == 0);
    for (uint32_t i = 0; i < max_bytes / 4; i++) {
        *(uint32_t *)&buf[i * 4] = pio_sm_get_blocking(qspi_rx_pio, qspi_rx_sm);
    }

    return max_bytes;
}

void disp_recv_init() {
    qspi_init(pio1, 0);
}

int disp_recv_blocking(void *buf, uint32_t size) {
    return qspi_get((uint8_t *)buf, size);
}
#endif // SR_ENABLE_INPUT_QSPI
