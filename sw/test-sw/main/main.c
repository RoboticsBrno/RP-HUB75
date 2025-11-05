#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"

#include "rphub75.h"
#include "colors.h"

// Potentiometer pins
#define POT1_GPIO 1
#define POT2_GPIO 2

// ADC parameters
#define ADC_WIDTH ADC_WIDTH_BIT_12
#define ADC_ATTEN ADC_ATTEN_DB_11

static const char *TAG = "RPHUB75";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting RPHUB75 example");
    spi_init();
    spi_set_internal_rx_capacity(0);
    /*display_init();
    ESP_LOGI(TAG, "Display initialized");
    fb_clear(0, color_red); // Clear framebuffer 0 to red
    ESP_LOGI(TAG, "Framebuffer 0 cleared to red");
    display_flip(0); // Show framebuffer 0
    ESP_LOGI(TAG, "Displaying framebuffer 0");
*/
    // fb_draw(0, 10, 10, &color_red, 1, 1); // Draw a red pixel at (10,10) in framebuffer 0

    ESP_LOGI(TAG, "Drawing gradient to framebuffer 1");
    /* Compute total bytes for the framebuffer using size_t to avoid overflow
       and ensure the multiplication is performed in size_t. */
    size_t buffer_size = (size_t)RP_HUB75_WIDTH * (size_t)RP_HUB75_HEIGHT * sizeof(rpio_rgb_t);
    rpio_rgb_t *buffer = pvPortMalloc(buffer_size);
    /*if (buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes for framebuffer", buffer_size);
        while (1)
        {
            vTaskDelay(portMAX_DELAY);
        }
    }
    for (int y = 0; y < RP_HUB75_HEIGHT; y++)
    {
        for (int x = 0; x < RP_HUB75_WIDTH; x++)
        {
            buffer[y * RP_HUB75_WIDTH + x] = rgb(x * 4, y * 4, 128); // Gradient
        }
    }
*/
    uint8_t i = 0;
    while (1)
    {
          ESP_LOGI(TAG, "Sending gradient data over SPI, iteration %d (bytes=%zu)", i++, buffer_size);
          /* sizeof(buffer) is the size of the pointer (4 or 8 bytes), not the allocated block.
              Pass the actual allocated size. */
          spi_send_data((uint8_t *)buffer, buffer_size);
        ESP_LOGI(TAG, "Data sent");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
