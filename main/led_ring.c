#include "led_ring.h"
#include "led_strip.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "LED_RING";

// Internal pixel buffer
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t brightness;  // Global brightness 0-100%
} pixel_buffer_t;

typedef struct {
    led_strip_handle_t strip;
    pixel_buffer_t *pixels;
    int num_leds;
    uint8_t global_brightness;
} led_ring_internal_t;

// Helper function to apply brightness
static void apply_brightness(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t brightness)
{
    *r = (*r * brightness) / 100;
    *g = (*g * brightness) / 100;
    *b = (*b * brightness) / 100;
}

// Initialize LED ring
static esp_err_t led_ring_init(LEDRing_t *self)
{
    led_ring_internal_t *internal = (led_ring_internal_t *)self->strip_handle;
    
    // LED strip configuration
    led_strip_config_t strip_config = {
        .strip_gpio_num = self->pin,
        .max_leds = self->num_leds,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    // RMT backend configuration
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };

    // Create LED strip
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &internal->strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
        return ret;
    }

    // Allocate pixel buffer
    internal->pixels = calloc(self->num_leds, sizeof(pixel_buffer_t));
    if (!internal->pixels) {
        ESP_LOGE(TAG, "Failed to allocate pixel buffer");
        return ESP_ERR_NO_MEM;
    }

    internal->num_leds = self->num_leds;
    internal->global_brightness = 100;  // Default 100% brightness

    // Clear all LEDs
    led_strip_clear(internal->strip);

    ESP_LOGI(TAG, "LED Ring initialized: %d LEDs on GPIO %d", self->num_leds, self->pin);
    return ESP_OK;
}

// Set all LEDs to the same color
static esp_err_t led_ring_set_all(LEDRing_t *self, uint8_t red, uint8_t green, uint8_t blue)
{
    led_ring_internal_t *internal = (led_ring_internal_t *)self->strip_handle;
    
    // Store colors in buffer
    for (int i = 0; i < internal->num_leds; i++) {
        internal->pixels[i].red = red;
        internal->pixels[i].green = green;
        internal->pixels[i].blue = blue;
    }

    // Apply to strip with brightness
    for (int i = 0; i < internal->num_leds; i++) {
        uint8_t r = red, g = green, b = blue;
        apply_brightness(&r, &g, &b, internal->global_brightness);
        led_strip_set_pixel(internal->strip, i, r, g, b);
    }

    return led_strip_refresh(internal->strip);
}

// Set individual pixel
static esp_err_t led_ring_set_pixel(LEDRing_t *self, int index, uint8_t red, uint8_t green, uint8_t blue)
{
    led_ring_internal_t *internal = (led_ring_internal_t *)self->strip_handle;
    
    if (index < 0 || index >= internal->num_leds) {
        ESP_LOGE(TAG, "Invalid pixel index: %d", index);
        return ESP_ERR_INVALID_ARG;
    }

    // Store color in buffer
    internal->pixels[index].red = red;
    internal->pixels[index].green = green;
    internal->pixels[index].blue = blue;

    // Apply with brightness
    uint8_t r = red, g = green, b = blue;
    apply_brightness(&r, &g, &b, internal->global_brightness);
    
    return led_strip_set_pixel(internal->strip, index, r, g, b);
}

// Set global brightness (0-100%)
static esp_err_t led_ring_set_brightness(LEDRing_t *self, uint8_t brightness_percent)
{
    led_ring_internal_t *internal = (led_ring_internal_t *)self->strip_handle;
    
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }

    internal->global_brightness = brightness_percent;
    ESP_LOGI(TAG, "Brightness set to %d%%", brightness_percent);

    // Reapply all pixels with new brightness
    for (int i = 0; i < internal->num_leds; i++) {
        uint8_t r = internal->pixels[i].red;
        uint8_t g = internal->pixels[i].green;
        uint8_t b = internal->pixels[i].blue;
        apply_brightness(&r, &g, &b, internal->global_brightness);
        led_strip_set_pixel(internal->strip, i, r, g, b);
    }

    return led_strip_refresh(internal->strip);
}

// Clear all LEDs (turn off)
static esp_err_t led_ring_clear(LEDRing_t *self)
{
    led_ring_internal_t *internal = (led_ring_internal_t *)self->strip_handle;
    
    // Clear buffer
    memset(internal->pixels, 0, internal->num_leds * sizeof(pixel_buffer_t));
    
    // Clear strip
    return led_strip_clear(internal->strip);
}

// Refresh the LED strip (send data)
static esp_err_t led_ring_refresh(LEDRing_t *self)
{
    led_ring_internal_t *internal = (led_ring_internal_t *)self->strip_handle;
    return led_strip_refresh(internal->strip);
}

// Constructor
LEDRing_t *led_ring_create(gpio_num_t pin, int num_leds)
{
    LEDRing_t *led_ring = malloc(sizeof(LEDRing_t));
    if (!led_ring) {
        ESP_LOGE(TAG, "Failed to allocate memory for LED Ring");
        return NULL;
    }

    // Allocate internal structure
    led_ring_internal_t *internal = malloc(sizeof(led_ring_internal_t));
    if (!internal) {
        ESP_LOGE(TAG, "Failed to allocate memory for LED Ring internal");
        free(led_ring);
        return NULL;
    }

    led_ring->pin = pin;
    led_ring->num_leds = num_leds;
    led_ring->strip_handle = internal;
    
    // Assign methods
    led_ring->init = led_ring_init;
    led_ring->set_all = led_ring_set_all;
    led_ring->set_pixel = led_ring_set_pixel;
    led_ring->set_brightness = led_ring_set_brightness;
    led_ring->clear = led_ring_clear;
    led_ring->refresh = led_ring_refresh;

    return led_ring;
}

// Destructor
void led_ring_destroy(LEDRing_t *led_ring)
{
    if (led_ring) {
        if (led_ring->strip_handle) {
            led_ring_internal_t *internal = (led_ring_internal_t *)led_ring->strip_handle;
            
            // Clear LEDs before destroying
            if (internal->strip) {
                led_strip_clear(internal->strip);
                led_strip_del(internal->strip);
            }
            
            if (internal->pixels) {
                free(internal->pixels);
            }
            
            free(internal);
        }
        free(led_ring);
    }
}
