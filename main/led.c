#include "led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "LED";

// LED method implementations
static void led_init(LED_t *self)
{
    gpio_reset_pin(self->pin);
    gpio_set_direction(self->pin, GPIO_MODE_OUTPUT);
    ESP_LOGI(TAG, "LED initialized on GPIO %d", self->pin);
}

static void led_blink(LED_t *self, int times)
{
    for (int i = 0; i < times; i++)
    {
        gpio_set_level(self->pin, 1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        gpio_set_level(self->pin, 0);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

static void led_on(LED_t *self)
{
    gpio_set_level(self->pin, 1);
}

static void led_off(LED_t *self)
{
    gpio_set_level(self->pin, 0);
}

// Constructor
LED_t *led_create(gpio_num_t pin)
{
    LED_t *led = malloc(sizeof(LED_t));
    if (!led)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for LED");
        return NULL;
    }

    led->pin = pin;
    led->init = led_init;
    led->blink = led_blink;
    led->on = led_on;
    led->off = led_off;

    return led;
}

// Destructor
void led_destroy(LED_t *led)
{
    if (led)
    {
        free(led);
    }
}
