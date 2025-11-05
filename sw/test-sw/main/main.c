#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"
#include <stdio.h>
#include "esp_task_wdt.h"
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

// Button pins for RGB control
#define BUTTON_RED_GPIO 18
#define BUTTON_GREEN_GPIO 16
#define BUTTON_BLUE_GPIO 42

static const char *TAG = "RPHUB75";

rpio_rgb_t current_color = {255, 255, 255}; // Start with white
// Color increment value
#define COLOR_INCREMENT 32

esp_err_t initialize_adc(void)
{
    ESP_LOGI(TAG, "Initializing ADC for potentiometers...");

    // Configure ADC width and attenuation
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC1_CHANNEL_0,
                              ADC_ATTEN); // GPIO 1 = ADC1_CHANNEL_0
    adc1_config_channel_atten(ADC1_CHANNEL_1,
                              ADC_ATTEN); // GPIO 2 = ADC1_CHANNEL_1

    ESP_LOGI(TAG,
             "ADC initialized for GPIO %d (Channel 0) and GPIO %d (Channel 1)",
             POT1_GPIO, POT2_GPIO);

    return ESP_OK;
}

esp_err_t initialize_buttons(void)
{
    ESP_LOGI(TAG, "Initializing buttons for RGB control...");

    // Configure button GPIOs as inputs with pull-up resistors
    gpio_config_t button_config = {.pin_bit_mask = (1ULL << BUTTON_RED_GPIO) |
                                                   (1ULL << BUTTON_GREEN_GPIO) |
                                                   (1ULL << BUTTON_BLUE_GPIO),
                                   .mode = GPIO_MODE_INPUT,
                                   .pull_up_en = GPIO_PULLUP_ENABLE,
                                   .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                   .intr_type = GPIO_INTR_DISABLE};

    esp_err_t ret = gpio_config(&button_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure buttons: 0x%x", ret);
        return ret;
    }

    ESP_LOGI(TAG,
             "Buttons initialized: Red(GPIO %d), Green(GPIO %d), Blue(GPIO %d)",
             BUTTON_RED_GPIO, BUTTON_GREEN_GPIO, BUTTON_BLUE_GPIO);

    return ESP_OK;
}

uint8_t read_potentiometer_value(int gpio_num)
{
    int adc_reading = 0;

    // Read ADC based on GPIO number
    switch (gpio_num)
    {
    case POT1_GPIO:
        adc_reading = adc1_get_raw(ADC1_CHANNEL_0);
        break;
    case POT2_GPIO:
        adc_reading = adc1_get_raw(ADC1_CHANNEL_1);
        break;
    default:
        ESP_LOGE(TAG, "Invalid GPIO for potentiometer: %d", gpio_num);
        return 0;
    }

    // Convert 12-bit ADC reading (0-4095) to 0-63 range for 64x64 display
    uint8_t mapped_value = (uint8_t)((adc_reading * RP_HUB75_WIDTH) / 4096);

    // Clamp to 0-63 range
    if (mapped_value >= RP_HUB75_WIDTH)
    {
        mapped_value = RP_HUB75_WIDTH - 1;
    }

    return mapped_value;
}

void update_color_state(void)
{
    static uint32_t last_button_check = 0;
    const uint32_t debounce_delay = 200; // 200ms debounce

    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Check if enough time has passed for debouncing
    if (current_time - last_button_check < debounce_delay)
    {
        return;
    }

    last_button_check = current_time;

    // Check red button (GPIO 18)
    if (gpio_get_level(BUTTON_RED_GPIO) ==
        0)
    { // Button pressed (active low with pull-up)
        current_color.r = (current_color.r + COLOR_INCREMENT) % 256;
        ESP_LOGI(TAG, "Red button pressed - R: %d", current_color.r);
    }

    // Check green button (GPIO 16)
    if (gpio_get_level(BUTTON_GREEN_GPIO) == 0)
    { // Button pressed
        current_color.g = (current_color.g + COLOR_INCREMENT) % 256;
        ESP_LOGI(TAG, "Green button pressed - G: %d", current_color.g);
    }

    // Check blue button (GPIO 42)
    if (gpio_get_level(BUTTON_BLUE_GPIO) == 0)
    { // Button pressed
        current_color.b = (current_color.b + COLOR_INCREMENT) % 256;
        ESP_LOGI(TAG, "Blue button pressed - B: %d", current_color.b);
    }
}

void draw_circle(rpio_rgb_t *fb, uint8_t x_center, uint8_t y_center,
                 rpio_rgb_t *color)
{
    uint8_t radius = 1;

    for (int y = -radius; y <= radius; y++)
    {
        for (int x = -radius; x <= radius; x++)
        {
            if (x * x + y * y <= radius * radius)
            {
                int draw_x = x_center + x;
                int draw_y = y_center + y;
                if (draw_x >= 0 && draw_x < RP_HUB75_WIDTH && draw_y >= 0 &&
                    draw_y < RP_HUB75_HEIGHT)
                {
                    int index = draw_y * RP_HUB75_WIDTH + draw_x;
                    fb[index].r = color->r;
                    fb[index].g = color->g;
                    fb[index].b = color->b;
                }
            }
        }
    }
}

void update_framebuffer(rpio_rgb_t *fb, uint8_t x_pos, uint8_t y_pos,
                        rpio_rgb_t *color)
{
    // Clear the framebuffer (set all pixels to black)
    //for (int i = 0; i < RP_HUB75_WIDTH * RP_HUB75_HEIGHT; i++)
    //{
    //    fb[i] = color_black;
    //}

    // Draw circle with the current color
    draw_circle(fb, x_pos, y_pos, color);
}

esp_err_t ret;

void app_main(void)
{
    ESP_LOGI(TAG, "Starting RPHUB75 example");
    spi_init();
    spi_set_internal_rx_capacity(0);
    // Initialize ADC for potentiometers
    ret = initialize_adc();
    if (ret != ESP_OK)
    {
        return;
    }

    // Initialize buttons for RGB control
    ret = initialize_buttons();
    if (ret != ESP_OK)
    {
        return;
    }

    esp_err_t wdt_add_ret = esp_task_wdt_add(NULL);
    if (wdt_add_ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Task registered with TWDT");
    }
    else
    {
        ESP_LOGW(TAG, "esp_task_wdt_add returned 0x%x", wdt_add_ret);
    }

    size_t buffer_size = (size_t)RP_HUB75_WIDTH * (size_t)RP_HUB75_HEIGHT * sizeof(rpio_rgb_t);
    rpio_rgb_t *buffer = pvPortMalloc(buffer_size);
    if (buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes for framebuffer", buffer_size);
        while (1)
        {
            vTaskDelay(portMAX_DELAY);
        }
    }
    for (int i = 0; i < buffer_size / sizeof(rpio_rgb_t); i++)
    {
        buffer[i] = color_black;
    }

    while (1)
    {

        uint8_t pot1_val = read_potentiometer_value(POT1_GPIO); // X position
        uint8_t pot2_val = read_potentiometer_value(POT2_GPIO); // Y position

        update_color_state();

        ESP_LOGI(TAG, "Pos(X: %d, Y: %d), Color(R: %d, G: %d, B: %d)", pot1_val,
                 pot2_val, current_color.r, current_color.g, current_color.b);

        update_framebuffer(buffer, pot1_val, pot2_val, &current_color);

        spi_send_data((uint8_t *)buffer, buffer_size);
        ESP_LOGI(TAG, "Data sent");

        esp_err_t wdt_ret = esp_task_wdt_reset();
        if (wdt_ret != ESP_OK)
        {
            ESP_LOGW(TAG, "esp_task_wdt_reset returned 0x%x", wdt_ret);
        }
    }
}
