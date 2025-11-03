#include "rotary_encoder.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

static const char *TAG = "ROTARY_ENCODER";

// Notification values
#define NOTIFY_ROTATION (1 << 0)
#define NOTIFY_BUTTON (1 << 1)

// Task for handling encoder events (runs in normal task context, not ISR)
static void rotary_encoder_task(void *arg)
{
    RotaryEncoder_t *encoder = (RotaryEncoder_t *)arg;
    uint32_t notification_value;

    ESP_LOGI(TAG, "Rotary encoder task started");

    while (1)
    {
        // Wait for notification from ISR
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &notification_value, portMAX_DELAY) == pdTRUE)
        {
            // Handle rotation event
            if (notification_value & NOTIFY_ROTATION)
            {
                if (encoder->on_rotation)
                {
                    encoder->on_rotation(encoder, encoder->position);
                }
            }

            // Handle button press event
            if (notification_value & NOTIFY_BUTTON)
            {
                // Debounce: wait a bit and check if button is still pressed
                vTaskDelay(pdMS_TO_TICKS(50));
                
                if (gpio_get_level(encoder->sw_pin) == 0)
                {
                    // Button is still pressed after debounce delay
                    if (encoder->on_button_press)
                    {
                        encoder->on_button_press(encoder);
                    }
                    
                    // Wait for button release to prevent multiple triggers
                    while (gpio_get_level(encoder->sw_pin) == 0)
                    {
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                    
                    // Additional delay after release for debouncing
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }
    }
}

// ISR handler for CLK pin (rotation detection)
static void IRAM_ATTR rotary_clk_isr_handler(void *arg)
{
    RotaryEncoder_t *encoder = (RotaryEncoder_t *)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    int clk_state = gpio_get_level(encoder->clk_pin);
    int dt_state = gpio_get_level(encoder->dt_pin);

    // Only trigger on CLK falling edge
    if (clk_state == 0 && encoder->last_clk_state == 1)
    {
        // Check DT state to determine direction
        if (dt_state == 1)
        {
            encoder->position++; // Clockwise
        }
        else
        {
            encoder->position--; // Counter-clockwise
        }

        // Notify task to handle callback
        if (encoder->task_handle != NULL)
        {
            xTaskNotifyFromISR(encoder->task_handle, NOTIFY_ROTATION, eSetBits, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }

    encoder->last_clk_state = clk_state;
}

// ISR handler for button press
static void IRAM_ATTR button_isr_handler(void *arg)
{
    RotaryEncoder_t *encoder = (RotaryEncoder_t *)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Button is active low (pressed = 0)
    // Only trigger on falling edge (button press), ISR is configured for NEGEDGE
    // The ISR itself only fires on falling edge, so we don't need to check previous state
    
    // Simple debouncing: check if button is actually low
    int button_state = gpio_get_level(encoder->sw_pin);
    if (button_state == 0)
    {
        // Notify task to handle callback
        if (encoder->task_handle != NULL)
        {
            xTaskNotifyFromISR(encoder->task_handle, NOTIFY_BUTTON, eSetBits, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

// Initialize rotary encoder
static esp_err_t rotary_encoder_init_impl(RotaryEncoder_t *self)
{
    ESP_LOGI(TAG, "Initializing rotary encoder...");
    ESP_LOGI(TAG, "CLK: GPIO%d, DT: GPIO%d, SW: GPIO%d", self->clk_pin, self->dt_pin, self->sw_pin);

    // Create task for handling callbacks (runs in normal context, not ISR)
    BaseType_t result = xTaskCreate(
        rotary_encoder_task,
        "rotary_encoder",
        4096,
        (void *)self,
        5, // Priority
        &self->task_handle);

    if (result != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create rotary encoder task");
        return ESP_FAIL;
    }

    // Configure CLK pin (with pull-up)
    gpio_config_t clk_config = {
        .pin_bit_mask = (1ULL << self->clk_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE};
    ESP_ERROR_CHECK(gpio_config(&clk_config));

    // Configure DT pin (with pull-up)
    gpio_config_t dt_config = {
        .pin_bit_mask = (1ULL << self->dt_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&dt_config));

    // Configure SW pin (button, with pull-up)
    gpio_config_t sw_config = {
        .pin_bit_mask = (1ULL << self->sw_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE // Trigger on button press (falling edge)
    };
    ESP_ERROR_CHECK(gpio_config(&sw_config));

    // Install GPIO ISR service (ignore error if already installed)
    gpio_install_isr_service(0);

    // Add ISR handlers
    gpio_isr_handler_add(self->clk_pin, rotary_clk_isr_handler, (void *)self);
    gpio_isr_handler_add(self->sw_pin, button_isr_handler, (void *)self);

    // Read initial CLK state
    self->last_clk_state = gpio_get_level(self->clk_pin);

    ESP_LOGI(TAG, "Rotary encoder initialized successfully");
    return ESP_OK;
}

// Get current position
static int rotary_encoder_get_position_impl(RotaryEncoder_t *self)
{
    return self->position;
}

// Reset position to zero
static void rotary_encoder_reset_position_impl(RotaryEncoder_t *self)
{
    self->position = 0;
}

// Check if button is pressed
static bool rotary_encoder_is_button_pressed_impl(RotaryEncoder_t *self)
{
    return self->button_pressed;
}

// Constructor
RotaryEncoder_t *rotary_encoder_create(gpio_num_t clk_pin, gpio_num_t dt_pin, gpio_num_t sw_pin)
{
    RotaryEncoder_t *encoder = malloc(sizeof(RotaryEncoder_t));
    if (!encoder)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for rotary encoder");
        return NULL;
    }

    encoder->clk_pin = clk_pin;
    encoder->dt_pin = dt_pin;
    encoder->sw_pin = sw_pin;
    encoder->position = 0;
    encoder->last_clk_state = 1;
    encoder->button_pressed = false;
    encoder->task_handle = NULL;
    encoder->on_rotation = NULL;
    encoder->on_button_press = NULL;

    // Bind methods
    encoder->init = rotary_encoder_init_impl;
    encoder->get_position = rotary_encoder_get_position_impl;
    encoder->reset_position = rotary_encoder_reset_position_impl;
    encoder->is_button_pressed = rotary_encoder_is_button_pressed_impl;

    return encoder;
}

// Set rotation callback
void rotary_encoder_set_rotation_callback(RotaryEncoder_t *encoder, rotary_callback_t callback)
{
    if (encoder)
    {
        encoder->on_rotation = callback;
    }
}

// Set button callback
void rotary_encoder_set_button_callback(RotaryEncoder_t *encoder, button_callback_t callback)
{
    if (encoder)
    {
        encoder->on_button_press = callback;
    }
}

// Destructor
void rotary_encoder_destroy(RotaryEncoder_t *encoder)
{
    if (encoder)
    {
        // Delete task if it exists
        if (encoder->task_handle != NULL)
        {
            vTaskDelete(encoder->task_handle);
        }

        // Remove ISR handlers
        gpio_isr_handler_remove(encoder->clk_pin);
        gpio_isr_handler_remove(encoder->sw_pin);

        free(encoder);
    }
}
