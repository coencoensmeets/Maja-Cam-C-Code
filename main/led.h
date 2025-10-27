#ifndef LED_H
#define LED_H

#include "driver/gpio.h"

// LED "Class" - Handles status LED operations
typedef struct LED_t {
    gpio_num_t pin;
    
    // Methods
    void (*init)(struct LED_t* self);
    void (*blink)(struct LED_t* self, int times);
    void (*on)(struct LED_t* self);
    void (*off)(struct LED_t* self);
} LED_t;

// Constructor
LED_t* led_create(gpio_num_t pin);

// Destructor
void led_destroy(LED_t* led);

#endif // LED_H
