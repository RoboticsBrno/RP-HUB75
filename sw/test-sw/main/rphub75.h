// rphub75.h
// Minimal header to provide SPI pin definitions and init prototype.
// Place any project-wide pin definitions here. Adjust pins as needed for your board.

#ifndef RPHUB75_H
#define RPHUB75_H

#include <stdint.h>
#include <rpio.h>
#include <esp_err.h>
#include <stddef.h>

// Default SPI pin assignments (change to match your wiring)
#define SPI_HOST SPI2_HOST
#define RP_PIN_MISO 7
#define RP_PIN_MOSI 12
#define RP_PIN_SCLK 13
#define RP_PIN_CS 14



#define RP_HUB75_DATA_BASE 0   // Base pin for R0, G0, B0, R1, G1, B1
#define RP_HUB75_ROWS_BASE 6   // Base pin for A, B, C, D, E
#define RP_HUB75_CTRL_BASE 11  // Base pin for LAT, OEn
#define RP_HUB75_CLK_PIN   13  // Pin for CLK

#define RP_HUB75_WIDTH  64  // Width of the HUB75 matrix in pixels
#define RP_HUB75_HEIGHT 64  // Height of the HUB75 matrix in pixels

#define RP_FB_COUNT 4 // Number of framebuffers available specified in firmware

rpio_rgb_t rgb(uint8_t r, uint8_t g, uint8_t b);
rpio_rgb_t rgba(uint8_t r, uint8_t g, uint8_t b, float a);
rpio_rgb_t hsv(uint8_t h, uint8_t s, uint8_t v);

// SPI functions
esp_err_t spi_init(void);
void spi_deinit(void);
void spi_print(void);

esp_err_t spi_send_and_receive(const uint8_t *tx, uint32_t tx_len, uint8_t *rx, uint32_t rx_len);
esp_err_t spi_send_data(const uint8_t *tx, uint32_t tx_len);
esp_err_t spi_read(uint32_t rx_len);
esp_err_t spi_set_internal_rx_capacity(uint32_t capacity);
const uint8_t *spi_get_last_rx(uint32_t *out_len);

void misc_hardware_info(void);
void misc_stat(void);

void noop_test(uint8_t i);


// Display functions
void display_init(void);
void display_deinit(void);
void display_flip(uint8_t fb_index);

// Framebuffer functions
void fb_clear(uint8_t fb_index, rpio_rgb_t color);
void fb_blit(uint8_t src_fb, uint8_t dst_fb,
             uint16_t src_x, uint16_t src_y,
             uint16_t dst_x, uint16_t dst_y,
             uint16_t w, uint16_t h);
void fb_draw(uint8_t fb_index, uint16_t x, uint16_t y,
             const rpio_rgb_t *bitmap, uint16_t w, uint16_t h);



#endif // RPHUB75_H
