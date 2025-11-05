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
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"

#include "rphub75.h"

static const char *TAG = "RPHUB75";
static spi_device_handle_t s_spi = NULL;
static SemaphoreHandle_t s_spi_lock = NULL;

/* Internal RX storage for polling-style reads. */
static uint8_t *s_internal_rx = NULL;
static size_t s_internal_rx_capacity = 0;
static size_t s_internal_rx_len = 0;

static inline float clampf(float val, float min, float max)
{
    return fminf(fmaxf(val, min), max);
}

// Color creation functions
rpio_rgb_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    rpio_rgb_t color = {.r = r, .g = g, .b = b};
    return color;
}

rpio_rgb_t rgba(uint8_t r, uint8_t g, uint8_t b, float a)
{
    rpio_rgb_t color = {.r = (uint8_t)roundf(clampf(r * a, 0.0f, 255.0f)), .g = (uint8_t)roundf(clampf(g * a, 0.0f, 255.0f)), .b = (uint8_t)roundf(clampf(b * a, 0.0f, 255.0f))};
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
    switch (region)
    {
    case 0:
        color.r = v;
        color.g = t;
        color.b = p;
        break;
    case 1:
        color.r = q;
        color.g = v;
        color.b = p;
        break;
    case 2:
        color.r = p;
        color.g = v;
        color.b = t;
        break;
    case 3:
        color.r = p;
        color.g = q;
        color.b = v;
        break;
    case 4:
        color.r = t;
        color.g = p;
        color.b = v;
        break;
    default:
        color.r = v;
        color.g = p;
        color.b = q;
        break;
    }
    return color;
}

// SPI functions

esp_err_t spi_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    spi_bus_config_t buscfg = {
        .miso_io_num = RP_PIN_MISO,
        .mosi_io_num = RP_PIN_MOSI,
        .sclk_io_num = RP_PIN_SCLK,

        .max_transfer_sz = 4096, // use default
        .flags = 0,
        .intr_flags = 0};

    // Initialize SPI bus
    esp_err_t ret = spi_bus_initialize(SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: 0x%x", ret);
        return ret;
    }

    ESP_LOGI(TAG, "SPI bus initialized successfully");
    ESP_LOGI(TAG, "Adding SPI device...");

    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0, // SPI mode 0
        .duty_cycle_pos = 0,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = 20 * 1000 * 1000, // 20 MHz
        .input_delay_ns = 0,
        .spics_io_num = RP_PIN_CS,
        .flags = 0,
        .queue_size = 3,
        .pre_cb = NULL,
        .post_cb = NULL};

    ret = spi_bus_add_device(SPI_HOST, &devcfg, &s_spi);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add SPI device: 0x%x", ret);
        spi_bus_free(SPI_HOST);
        return ret;
    }

    /* create mutex for SPI operations */
    if (s_spi_lock == NULL)
    {
        s_spi_lock = xSemaphoreCreateMutex();
        if (s_spi_lock == NULL)
        {
            ESP_LOGW(TAG, "Failed to create spi mutex");
        }
    }

    ESP_LOGI(TAG, "SPI device added successfully");
    return ESP_OK;
}

esp_err_t spi_send_and_receive(const uint8_t *tx, uint32_t tx_len, uint8_t *rx, uint32_t rx_len)
{
    ESP_LOGI(TAG, "spi_send_and_receive: start");
    if (s_spi == NULL)
    {
        ESP_LOGI(TAG, "spi_send_and_receive: err");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;

    if (s_spi_lock)
        xSemaphoreTake(s_spi_lock, portMAX_DELAY);

    uint32_t total = tx_len;
    if (rx_len > total)
        total = rx_len;

    for (uint32_t i = 0; i < total; ++i)
    {
        if (i%1000 == 0){

            esp_err_t wdt_ret = esp_task_wdt_reset();
            if (wdt_ret != ESP_OK)
            {
                ESP_LOGW(TAG, "esp_task_wdt_reset returned 0x%x", wdt_ret);
            }
        }

        uint8_t tx_byte = 0x00;
        if (tx != NULL && i < tx_len)
            tx_byte = tx[i];

        uint8_t rx_byte = 0x00;

        spi_transaction_t t = {0};
        t.length = 8; // bits (one byte)
        t.tx_buffer = &tx_byte;
        t.rxlength = 8;
        t.rx_buffer = &rx_byte;

        ret = spi_device_queue_trans(s_spi, &t, portMAX_DELAY);

        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "spi_send_and_receive: byte %lu transmit failed: %s", (unsigned long)i, esp_err_to_name(ret));
            break;
        }

        if (rx != NULL && i < rx_len)
            rx[i] = rx_byte;
    }

    if (s_spi_lock)
        xSemaphoreGive(s_spi_lock);
    ESP_LOGI(TAG, "spi_send_and_receive: end");
    return ret;
}

esp_err_t spi_set_internal_rx_capacity(uint32_t capacity)
{
    if (capacity == 0)
    {
        if (s_internal_rx)
        {
            heap_caps_free(s_internal_rx);
            s_internal_rx = NULL;
        }
        s_internal_rx_capacity = 0;
        s_internal_rx_len = 0;
        return ESP_OK;
    }

    uint8_t *buf = heap_caps_calloc(capacity, 1, MALLOC_CAP_DMA);
    if (buf == NULL)
        return ESP_ERR_NO_MEM;

    // replace existing buffer
    if (s_internal_rx)
        heap_caps_free(s_internal_rx);
    s_internal_rx = buf;
    s_internal_rx_capacity = capacity;
    s_internal_rx_len = 0;
    return ESP_OK;
}

const uint8_t *spi_get_last_rx(uint32_t *out_len)
{
    if (out_len)
        *out_len = s_internal_rx_len;
    return s_internal_rx;
}

esp_err_t spi_send_data(const uint8_t *tx, uint32_t tx_len)
{
    ESP_LOGI(TAG, "spi_send_data: tx_len=%u", (unsigned)tx_len);
    if (s_internal_rx_capacity == 0)
    {
        return spi_send_and_receive(tx, tx_len, NULL, 0);
    }
    esp_err_t ret = spi_send_and_receive(tx, tx_len, s_internal_rx, s_internal_rx_capacity);
    if (ret == ESP_OK)
    {
        s_internal_rx_len = s_internal_rx_capacity;
    }
    return ret;
}

esp_err_t spi_read(uint32_t rx_len)
{
    if (s_internal_rx == NULL || s_internal_rx_capacity == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = spi_send_and_receive(NULL, 0, s_internal_rx, rx_len);
    if (ret != ESP_OK)
    {
        return ret;
    }

    // We read exactly the requested capacity bytes.
    s_internal_rx_len = rx_len;

    return ESP_OK;
}

void spi_print(void)
{
    if (s_internal_rx == NULL || s_internal_rx_len == 0)
    {
        ESP_LOGI(TAG, "spi_print: no data");
        return;
    }

    char *line = heap_caps_calloc(s_internal_rx_len * 3 + 1, 1, MALLOC_CAP_8BIT);
    if (line == NULL)
    {
        ESP_LOGW(TAG, "spi_print: allocation failed");
        return;
    }
    char *p = line;
    for (size_t i = 0; i < s_internal_rx_len; ++i)
    {
        int n = sprintf(p, "%02X ", s_internal_rx[i]);
        p += n;
    }
    ESP_LOGI(TAG, "spi RX (%lu bytes): %s", s_internal_rx_len, line);
    heap_caps_free(line);
}

void spi_deinit(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    if (s_spi)
    {
        spi_bus_remove_device(s_spi);
        s_spi = NULL;
    }

    spi_bus_free(SPI_HOST);

    if (s_spi_lock)
    {
        vSemaphoreDelete(s_spi_lock);
        s_spi_lock = NULL;
    }

    ESP_LOGI(TAG, "SPI deinitialized");
}

void misc_hardware_info(void)
{
    uint8_t buffer[2];
    buffer[0] = rpio_ctype_misc;
    buffer[1] = rpio_misc_hwinfo_cmd;

    esp_err_t ret = spi_send_data(buffer, sizeof(buffer));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "misc_hwinfo: spi_send_data failed: %s", esp_err_to_name(ret));
    }
}

void misc_stat(void)
{
    uint8_t buffer[2];
    buffer[0] = rpio_ctype_misc;
    buffer[1] = rpio_misc_stat_cmd;

    esp_err_t ret = spi_send_data(buffer, sizeof(buffer));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "misc_stat: spi_send_data failed: %s", esp_err_to_name(ret));
    }
}

void noop_test(uint8_t i)
{
    /* Large buffer - allocate on the heap to avoid stack overflow in RTOS tasks. */
    const size_t buf_size = 4096*3;
    uint8_t *buffer = heap_caps_calloc(buf_size, 1, MALLOC_CAP_DMA);
    if (buffer == NULL)
    {
        ESP_LOGE(TAG, "noop_test: allocation failed for %lu bytes", (unsigned long)buf_size);
        return;
    }

    buffer[0] = 0xF0;
    buffer[1] = 0xE0;
    buffer[2] = 0xD0;
    buffer[3] = 0xC0;
    buffer[buf_size - 2] = i;
    buffer[buf_size - 1] = 0xAA;
    ESP_LOGI(TAG, "noop_test: sending noop test data %lu bytes", (unsigned long)buf_size);

    esp_err_t ret = spi_send_data(buffer, buf_size);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "noop_test: spi_send_data failed: %s", esp_err_to_name(ret));
    }

    heap_caps_free(buffer);
}

// Display functions
void display_init(void)
{
    rpio_hub75_init_t init_struct = {
        .data_base = RP_HUB75_DATA_BASE,
        .rows_base = RP_HUB75_ROWS_BASE,
        .ctrl_base = RP_HUB75_CTRL_BASE,
        .clk_pin = RP_HUB75_CLK_PIN,
        .width = RP_HUB75_WIDTH,
        .height = RP_HUB75_HEIGHT,
    };

    uint8_t buffer[2 + sizeof(init_struct)];
    buffer[0] = rpio_ctype_hub75;
    buffer[1] = rpio_hub75_init_cmd;
    memcpy(&buffer[2], &init_struct, sizeof(init_struct));

    esp_err_t ret = spi_send_data(buffer, sizeof(buffer));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "display_init: spi_send_data failed: %s", esp_err_to_name(ret));
    }
}

void display_deinit(void)
{
    uint8_t buffer[2];
    buffer[0] = rpio_ctype_hub75;
    buffer[1] = rpio_hub75_deinit_cmd;

    esp_err_t ret = spi_send_data(buffer, sizeof(buffer));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "display_deinit: spi_send_data failed: %s", esp_err_to_name(ret));
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
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "display_flip: spi_send_data failed: %s", esp_err_to_name(ret));
    }
}

// Framebuffer functions
void fb_clear(uint8_t fb_index, rpio_rgb_t color)
{
    if (fb_index >= RP_FB_COUNT)
    {
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
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "fb_clear: spi_send_data failed: %s", esp_err_to_name(ret));
    }
}

void fb_blit(uint8_t src_fb, uint8_t dst_fb,
             uint16_t src_x, uint16_t src_y,
             uint16_t dst_x, uint16_t dst_y,
             uint16_t w, uint16_t h)
{
    if (src_fb >= RP_FB_COUNT)
    {
        ESP_LOGE(TAG, "fb_blit: src_fb %u out of range (max %u)", (unsigned)src_fb, (unsigned)RP_FB_COUNT);
        return;
    }
    if (dst_fb >= RP_FB_COUNT)
    {
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
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "fb_blit: spi_send_data failed: %s", esp_err_to_name(ret));
    }
}

void fb_draw(uint8_t fb_index, uint16_t x, uint16_t y,
             const rpio_rgb_t *bitmap, uint16_t w, uint16_t h)
{
    if (fb_index >= RP_FB_COUNT)
    {
        ESP_LOGE(TAG, "fb_draw: fb_index %u out of range (max %u)", (unsigned)fb_index, (unsigned)RP_FB_COUNT);
        return;
    }

    /* compute sizes safely */
    size_t struct_size = sizeof(rpio_fb_draw_t);
    size_t elems = (size_t)w * (size_t)h;
    if (w != 0 && elems / (size_t)w != (size_t)h)
    {
        ESP_LOGE(TAG, "fb_draw: width*height overflow");
        return;
    }

    size_t bitmap_size = elems * sizeof(rpio_rgb_t);
    if (bitmap_size > 0 && bitmap == NULL)
    {
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
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "fb_draw: header send failed: %s", esp_err_to_name(ret));
        return;
    }

    if (bitmap_size > 0)
    {
        ret = spi_send_data((const uint8_t *)bitmap, bitmap_size);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "fb_draw: bitmap stream failed: %s", esp_err_to_name(ret));
            return;
        }
    }
}