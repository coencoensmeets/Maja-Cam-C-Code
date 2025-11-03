#ifndef LED_RING_H
#define LED_RING_H

#include "driver/gpio.h"
#include "esp_err.h"

#define LED_RING_COUNT 40  // Number of WS2812B LEDs in the ring

// LED Ring "Class" - Handles WS2812B LED ring operations
typedef struct LEDRing_t {
    gpio_num_t pin;
    int num_leds;
    void *strip_handle;  // RMT strip handle
    
    // Methods
    esp_err_t (*init)(struct LEDRing_t* self);
    esp_err_t (*set_all)(struct LEDRing_t* self, uint8_t red, uint8_t green, uint8_t blue);
    esp_err_t (*set_pixel)(struct LEDRing_t* self, int index, uint8_t red, uint8_t green, uint8_t blue);
    esp_err_t (*set_brightness)(struct LEDRing_t* self, uint8_t brightness_percent);
    esp_err_t (*clear)(struct LEDRing_t* self);
    esp_err_t (*refresh)(struct LEDRing_t* self);
} LEDRing_t;

// Constructor
LEDRing_t* led_ring_create(gpio_num_t pin, int num_leds);

// Destructor
void led_ring_destroy(LEDRing_t* led_ring);

#endif // LED_RING_H
