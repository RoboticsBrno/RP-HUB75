// rpio.c
// SPI initialization implementation that uses pin definitions from ../rphub75.h

#include "esp_log.h"
#include <stdio.h>
#include "sdkconfig.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_mac.h"
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "rphub75.h"

static const char *TAG = "RPHUB75";
static spi_device_handle_t s_spi = NULL;
static SemaphoreHandle_t s_spi_lock = NULL;
static size_t s_max_transfer = 0; // set from bus config in spi_init

static inline float clampf(float val, float min, float max) {
    return fminf(fmaxf(val, min), max);
}

// Color creation functions
rpio_rgb_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    rpio_rgb_t color = { .r = r, .g = g, .b = b };
    return color;
}

rpio_rgb_t rgba(uint8_t r, uint8_t g, uint8_t b, float a)
{
    rpio_rgb_t color = { .r = (uint8_t)roundf(clampf(r * a, 0.0f, 255.0f)), .g = (uint8_t)roundf(clampf(g * a, 0.0f, 255.0f)), .b = (uint8_t)roundf(clampf(b * a, 0.0f, 255.0f)) };
    return color;
}

rpio_rgb_t hsv(uint8_t h, uint8_t s, uint8_t v)
{
    uint8_t region = h / 43;
    uint8_t remainder = (h - (region * 43)) * 6;

    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    rpio_rgb_t color;
    switch (region) {
        case 0:
            color.r = v; color.g = t; color.b = p;
            break;
        case 1:
            color.r = q; color.g = v; color.b = p;
            break;
        case 2:
            color.r = p; color.g = v; color.b = t;
            break;
        case 3:
            color.r = p; color.g = q; color.b = v;
            break;
        case 4:
            color.r = t; color.g = p; color.b = v;
            break;
        default:
            color.r = v; color.g = p; color.b = q;
            break;
    }
    return color;
}


// SPI functions
void spi_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    spi_bus_config_t buscfg = {
        .mosi_io_num = RP_PIN_MOSI,
        .miso_io_num = RP_PIN_MISO,
        .sclk_io_num = RP_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        printf("spi_bus_initialize failed: %s\n", esp_err_to_name(ret));
        return;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1 * 1000 * 1000, // 1 MHz
        .mode = 0,
        .spics_io_num = RP_PIN_CS,
        .queue_size = 3,
    };

    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &s_spi);
    if (ret != ESP_OK) {
        printf("spi_bus_add_device failed: %s\n", esp_err_to_name(ret));
        return;
    }

    // Optionally set CS as output if user expects manual control
    gpio_set_direction(RP_PIN_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(RP_PIN_CS, 1);

    /* remember bus max transfer size and create mutex for thread-safety */
    s_max_transfer = buscfg.max_transfer_sz ? buscfg.max_transfer_sz : 4096;
    s_spi_lock = xSemaphoreCreateMutex();
    if (!s_spi_lock) {
        ESP_LOGW(TAG, "spi_init: failed to create spi mutex");
    }

    ESP_LOGI(TAG, "SPI initialized on MOSI=%d MISO=%d SCLK=%d CS=%d",
        RP_PIN_MOSI, RP_PIN_MISO, RP_PIN_SCLK, RP_PIN_CS);
}

void spi_deinit(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    if (s_spi) {
        spi_bus_remove_device(s_spi);
        s_spi = NULL;
    }

    spi_bus_free(SPI2_HOST);

    if (s_spi_lock) {
        vSemaphoreDelete(s_spi_lock);
        s_spi_lock = NULL;
    }

    ESP_LOGI(TAG, "SPI deinitialized");
}

esp_err_t spi_send_data(const uint8_t *data, size_t length)
{
    if (!s_spi) {
        ESP_LOGE(TAG, "spi_send_data: SPI not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (length > 0 && data == NULL) {
        ESP_LOGE(TAG, "spi_send_data: NULL data with non-zero length");
        return ESP_ERR_INVALID_ARG;
    }

    size_t max_chunk = s_max_transfer ? s_max_transfer : 4096;
    size_t remaining = length;
    const uint8_t *ptr = data;
    esp_err_t last_ret = ESP_OK;

    if (s_spi_lock) {
        if (xSemaphoreTake(s_spi_lock, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGW(TAG, "spi_send_data: failed to take spi mutex");
            return ESP_ERR_TIMEOUT;
        }
    }

    while (remaining > 0) {
        size_t chunk = remaining > max_chunk ? max_chunk : remaining;

        spi_transaction_t trans = { 0 };
        trans.length = (uint32_t)(chunk * 8);
        trans.tx_buffer = ptr;

        last_ret = spi_device_transmit(s_spi, &trans);
        if (last_ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI transmit failed: %s", esp_err_to_name(last_ret));
            break;
        }

        remaining -= chunk;
        ptr += chunk;
    }

    if (s_spi_lock) {
        xSemaphoreGive(s_spi_lock);
    }

    return last_ret;
}

// Display functions
void display_init(void)
{
    rpio_hub75_init_t init_struct = {
        .data_base = 0,
        .rows_base = 6,
        .ctrl_base = 11,
        .clk_pin = 13,
    };

    uint8_t buffer[2 + sizeof(init_struct)];
    buffer[0] = rpio_ctype_hub75;
    buffer[1] = rpio_hub75_init_cmd;
    memcpy(&buffer[2], &init_struct, sizeof(init_struct));

    esp_err_t ret = spi_send_data(buffer, sizeof(buffer));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "display_init: spi_send_data failed: %s", esp_err_to_name(ret));
    }
}

void display_flip(uint8_t fb_index)
{
    rpio_hub75_flip_t flip_struct = {
        .fb = fb_index,
    };

    uint8_t buffer[2 + sizeof(flip_struct)];
    buffer[0] = rpio_ctype_hub75;
    buffer[1] = rpio_hub75_flip_cmd;
    memcpy(&buffer[2], &flip_struct, sizeof(flip_struct));

    esp_err_t ret = spi_send_data(buffer, sizeof(buffer));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "display_flip: spi_send_data failed: %s", esp_err_to_name(ret));
    }
}

// Framebuffer functions
void fb_clear(uint8_t fb_index, rpio_rgb_t color)
{
    if (fb_index >= RP_FB_COUNT) {
        ESP_LOGE(TAG, "fb_clear: fb_index %u out of range (max %u)", (unsigned)fb_index, (unsigned)RP_FB_COUNT);
        return;
    }
    rpio_fb_clear_t clear_struct = {
        .color = color,
        .fb = fb_index,
    };

    uint8_t buffer[2 + sizeof(clear_struct)];
    buffer[0] = rpio_ctype_fb;
    buffer[1] = rpio_fb_clear_cmd;
    memcpy(&buffer[2], &clear_struct, sizeof(clear_struct));

    esp_err_t ret = spi_send_data(buffer, sizeof(buffer));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "fb_clear: spi_send_data failed: %s", esp_err_to_name(ret));
    }
}

void fb_blit(uint8_t src_fb, uint8_t dst_fb,
             uint16_t src_x, uint16_t src_y,
             uint16_t dst_x, uint16_t dst_y,
             uint16_t w, uint16_t h)
{
    if (src_fb >= RP_FB_COUNT) {
        ESP_LOGE(TAG, "fb_blit: src_fb %u out of range (max %u)", (unsigned)src_fb, (unsigned)RP_FB_COUNT);
        return;
    }
    if (dst_fb >= RP_FB_COUNT) {
        ESP_LOGE(TAG, "fb_blit: dst_fb %u out of range (max %u)", (unsigned)dst_fb, (unsigned)RP_FB_COUNT);
        return;
    }

    rpio_fb_blit_t blit_struct = {
        .src_x = src_x,
        .src_y = src_y,
        .dst_x = dst_x,
        .dst_y = dst_y,
        .w = w,
        .h = h,
        .src_fb = src_fb,
        .dst_fb = dst_fb,
    };

    uint8_t buffer[2 + sizeof(blit_struct)];
    buffer[0] = rpio_ctype_fb;
    buffer[1] = rpio_fb_blit_cmd;
    memcpy(&buffer[2], &blit_struct, sizeof(blit_struct));

    esp_err_t ret = spi_send_data(buffer, sizeof(buffer));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "fb_blit: spi_send_data failed: %s", esp_err_to_name(ret));
    }
}

void fb_draw(uint8_t fb_index, uint16_t x, uint16_t y,
             const rpio_rgb_t *bitmap, uint16_t w, uint16_t h)
{
    if (fb_index >= RP_FB_COUNT) {
        ESP_LOGE(TAG, "fb_draw: fb_index %u out of range (max %u)", (unsigned)fb_index, (unsigned)RP_FB_COUNT);
        return;
    }

    /* compute sizes safely */
    size_t struct_size = sizeof(rpio_fb_draw_t);
    size_t elems = (size_t)w * (size_t)h;
    if (w != 0 && elems / (size_t)w != (size_t)h) {
        ESP_LOGE(TAG, "fb_draw: width*height overflow");
        return;
    }

    size_t bitmap_size = elems * sizeof(rpio_rgb_t);
    if (bitmap_size > 0 && bitmap == NULL) {
        ESP_LOGE(TAG, "fb_draw: bitmap is NULL");
        return;
    }

    /* send header (command + struct) first, then stream bitmap in chunks */
    uint8_t header[2 + sizeof(rpio_fb_draw_t)];
    rpio_fb_draw_t draw_struct = {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
        .fb = fb_index,
    };
    header[0] = rpio_ctype_fb;
    header[1] = rpio_fb_draw_cmd;
    memcpy(&header[2], &draw_struct, struct_size);

    esp_err_t ret = spi_send_data(header, 2 + struct_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "fb_draw: header send failed: %s", esp_err_to_name(ret));
        return;
    }

    if (bitmap_size > 0) {
        ret = spi_send_data((const uint8_t *)bitmap, bitmap_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "fb_draw: bitmap stream failed: %s", esp_err_to_name(ret));
            return;
        }
    }
}