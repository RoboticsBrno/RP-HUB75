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

// Button pins for platformer controls
#define BUTTON_JUMP_GPIO 16  // Red button - Jump
#define BUTTON_RIGHT_GPIO 42 // Green button - Move Right
#define BUTTON_LEFT_GPIO 18  // Blue button - Move Left

// Platformer physics parameters
#define GRAVITY 100
#define PLAYER_JUMP_SPD 40.0f
#define PLAYER_HOR_SPD 20.0f
// Player structure
typedef struct
{
    float position_x;
    float position_y;
    float speed;
    bool canJump;
} Player;

// Environment item structure
typedef struct
{
    int x;
    int y;
    int width;
    int height;
} EnvItem;

// Global player instance
Player player = {32.0f, 20.0f, 0.0f, false};

// Environment items (platforms)
EnvItem envItems[] = {
    {0, 56, 64, 8},  // Ground platform
    {16, 44, 32, 4}, // Middle platform
    {8, 32, 16, 4},  // Left platform
    {40, 32, 16, 4}  // Right platform
};
int envItemsLength = sizeof(envItems) / sizeof(envItems[0]);

static const char *TAG = "RPHUB75";

rpio_rgb_t current_color = {255, 255, 255}; // Start with white
// Color increment value
#define COLOR_INCREMENT 32

esp_err_t initialize_buttons(void)
{
    ESP_LOGI(TAG, "Initializing buttons for platformer controls...");

    gpio_config_t button_config = {.pin_bit_mask = (1ULL << BUTTON_JUMP_GPIO) |
                                                   (1ULL << BUTTON_RIGHT_GPIO) |
                                                   (1ULL << BUTTON_LEFT_GPIO),
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

    ESP_LOGI(
        TAG,
        "Buttons initialized: Jump(GPIO %d), Right(GPIO %d), Left(GPIO %d)",
        BUTTON_JUMP_GPIO, BUTTON_RIGHT_GPIO, BUTTON_LEFT_GPIO);

    return ESP_OK;
}

void update_player(Player *player, EnvItem *envItems, int envItemsLength,
                   float delta)
{
    // Read button states
    bool leftPressed = (gpio_get_level(BUTTON_LEFT_GPIO) == 0);
    bool rightPressed = (gpio_get_level(BUTTON_RIGHT_GPIO) == 0);
    bool jumpPressed = (gpio_get_level(BUTTON_JUMP_GPIO) == 0);

    // Horizontal movement
    if (leftPressed)
        player->position_x -= PLAYER_HOR_SPD * delta;
    if (rightPressed)
        player->position_x += PLAYER_HOR_SPD * delta;

    // Jumping
    if (jumpPressed && player->canJump)
    {
        player->speed = -PLAYER_JUMP_SPD;
        player->canJump = false;
    }

    // Apply gravity and check collisions
    bool hitObstacle = false;

    // Temporary position for collision checking
    float new_y = player->position_y + player->speed * delta;

    for (int i = 0; i < envItemsLength; i++)
    {
        EnvItem *ei = &envItems[i];

        // Check if player would collide with this platform
        // Player is 8x8 pixels, positioned at center-bottom
        float player_left = player->position_x - 4;
        float player_right = player->position_x + 4;
        float player_bottom = new_y + 4; // Bottom of player

        // Platform bounds
        float platform_left = ei->x;
        float platform_right = ei->x + ei->width;
        float platform_top = ei->y;

        // Check if player is above platform and falling onto it
        if (player_bottom >= platform_top &&
            player->position_y + 4 <= platform_top && // Was above platform
            player_right > platform_left && player_left < platform_right &&
            player->speed >= 0)
        { // Only check when falling

            hitObstacle = true;
            player->speed = 0.0f;
            player->position_y =
                platform_top - 4; // Position player on top of platform
            break;
        }
    }

    if (!hitObstacle)
    {
        player->position_y = new_y;
        player->speed += GRAVITY * delta;
        player->canJump = false;
    }
    else
    {
        player->canJump = true;
    }

    // Keep player within screen bounds
    if (player->position_x < 4)
        player->position_x = 4;
    if (player->position_x > RP_HUB75_WIDTH - 4)
        player->position_x = RP_HUB75_WIDTH - 4;
    if (player->position_y > RP_HUB75_HEIGHT - 4)
    {
        player->position_y = RP_HUB75_HEIGHT - 4;
        player->speed = 0;
        player->canJump = true;
    }
}

void draw_rectangle(rpio_rgb_t *fb, int x, int y, int width, int height,
                    uint8_t r, uint8_t g, uint8_t b)
{
    for (int py = y; py < y + height && py < RP_HUB75_HEIGHT; py++)
    {
        for (int px = x; px < x + width && px < RP_HUB75_WIDTH; px++)
        {
            if (px >= 0 && py >= 0)
            {
                int index = py * RP_HUB75_WIDTH + px;
                fb[index].r = r;
                fb[index].g = g;
                fb[index].b = b;
            }
        }
    }
}

void update_framebuffer(rpio_rgb_t *fb, Player *player, EnvItem *envItems,
                        int envItemsLength)
{
    // Clear framebuffer (transparent/black background)
    memset(fb, 0, sizeof(rpio_rgb_t) * RP_HUB75_WIDTH * RP_HUB75_HEIGHT);

    // Draw platforms (gray)
    for (int i = 0; i < envItemsLength; i++)
    {
        draw_rectangle(fb, envItems[i].x, envItems[i].y, envItems[i].width,
                       envItems[i].height, 128, 128, 128); // Gray color
    }

    // Draw player (red) - 8x8 pixels centered at player position
    int player_x = (int)player->position_x - 4;
    int player_y = (int)player->position_y - 4;
    draw_rectangle(fb, player_x, player_y, 8, 8, 255, 0, 0); // Red color
}
esp_err_t ret;

void app_main(void)
{
    ESP_LOGI(TAG, "Starting RPHUB75 example");
    spi_init();
    spi_set_internal_rx_capacity(0);
    // Initialize ADC for potentiometers
    // Initialize buttons for platformer controls
    ret = initialize_buttons();
    if (ret != ESP_OK)
    {
        return;
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
    TickType_t last_frame_time = xTaskGetTickCount();
    const TickType_t frame_delay = pdMS_TO_TICKS(16); // ~60 FPS
    int frame_counter = 0;

    {
        esp_err_t wdt_ret = esp_task_wdt_add(NULL);
        if (wdt_ret != ESP_OK)
        {
            ESP_LOGW(TAG, "esp_task_wdt_add returned 0x%x", wdt_ret);
        }
    }
    while (1)
    {
        TickType_t current_time = xTaskGetTickCount();
        float delta_time = (float)(current_time - last_frame_time) *
                           portTICK_PERIOD_MS / 1000.0f;
        last_frame_time = current_time;
        frame_counter++;

        update_player(&player, envItems, envItemsLength, delta_time);

        update_framebuffer(buffer, &player, envItems, envItemsLength);

        spi_send_data((uint8_t *)buffer,
                      RP_HUB75_WIDTH * RP_HUB75_HEIGHT * sizeof(rpio_rgb_t));
    }
}
