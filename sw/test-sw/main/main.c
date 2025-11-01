#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "rphub75.h"
#include "colors.h"

static const char *TAG = "RPHUB75";

void app_main(void)
{
    spi_init();
    display_init();
    fb_clear(0, color_black); // Clear framebuffer 0 to black
    display_flip(0); // Show framebuffer 0

    // fb_draw(0, 10, 10, &color_red, 1, 1); // Draw a red pixel at (10,10) in framebuffer 0

    rpio_rgb_t buffer[64 * 64];
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            buffer[y * 64 + x] = rgb(x * 4, y * 4, 128); // Gradient
        }
    }
    fb_draw(1, 0, 0, buffer, 64, 64); // Draw 64x64 gradient at (0,0) in framebuffer 1
    display_flip(1); // Show framebuffer 1

    while (1) {
        ESP_LOGI(TAG, "Hello");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
