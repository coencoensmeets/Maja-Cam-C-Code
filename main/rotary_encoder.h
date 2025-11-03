#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

// Forward declaration
typedef struct RotaryEncoder_t RotaryEncoder_t;

// Callback function types
typedef void (*rotary_callback_t)(RotaryEncoder_t *self, int position);
typedef void (*button_callback_t)(RotaryEncoder_t *self);

// Rotary Encoder "Class"
typedef struct RotaryEncoder_t
{
    gpio_num_t clk_pin;
    gpio_num_t dt_pin;
    gpio_num_t sw_pin;

    volatile int position;
    volatile int last_clk_state;
    volatile bool button_pressed;

    // Task handle for deferred processing
    TaskHandle_t task_handle;

    // Callbacks
    rotary_callback_t on_rotation;
    button_callback_t on_button_press;

    // Methods
    esp_err_t (*init)(struct RotaryEncoder_t *self);
    int (*get_position)(struct RotaryEncoder_t *self);
    void (*reset_position)(struct RotaryEncoder_t *self);
    bool (*is_button_pressed)(struct RotaryEncoder_t *self);
} RotaryEncoder_t;

// Constructor
RotaryEncoder_t *rotary_encoder_create(gpio_num_t clk_pin, gpio_num_t dt_pin, gpio_num_t sw_pin);

// Destructor
void rotary_encoder_destroy(RotaryEncoder_t *encoder);

// Set callbacks
void rotary_encoder_set_rotation_callback(RotaryEncoder_t *encoder, rotary_callback_t callback);
void rotary_encoder_set_button_callback(RotaryEncoder_t *encoder, button_callback_t callback);

#endif // ROTARY_ENCODER_H
