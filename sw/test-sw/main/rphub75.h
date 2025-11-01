// rphub75.h
// Minimal header to provide SPI pin definitions and init prototype.
// Place any project-wide pin definitions here. Adjust pins as needed for your board.

#ifndef RPHUB75_H
#define RPHUB75_H

#include <stdint.h>
#include <rpio.h>

// Default SPI pin assignments (change to match your wiring)
#define RP_PIN_MOSI  11
#define RP_PIN_MISO  10
#define RP_PIN_SCLK  12
#define RP_PIN_CS    13

#define RP_FB_COUNT 4 // Number of framebuffers available specified in firmware

typedef struct rpio_rgb rpio_rgb_t;
typedef struct rpio_hub75_init rpio_hub75_init_t;
typedef struct rpio_hub75_flip rpio_hub75_flip_t;
typedef struct rpio_fb_clear rpio_fb_clear_t;
typedef struct rpio_fb_blit rpio_fb_blit_t;
typedef struct rpio_fb_draw rpio_fb_draw_t;
typedef struct rpio_fb_read rpio_fb_read_t;

rpio_rgb_t rgb(uint8_t r, uint8_t g, uint8_t b);
rpio_rgb_t rgba(uint8_t r, uint8_t g, uint8_t b, float a);
rpio_rgb_t hsv(uint8_t h, uint8_t s, uint8_t v);

// SPI functions
void spi_init(void);
void spi_deinit(void);

// Display functions
void display_init(void);
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
