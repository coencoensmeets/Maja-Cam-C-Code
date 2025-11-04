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
                // Enhanced debounce: wait and verify button is actually pressed
                vTaskDelay(pdMS_TO_TICKS(150)); // Increased from 100ms
                
                // Check multiple times to ensure stable press
                int stable_press_count = 0;
                const int total_checks = 8; // Increased from 5
                const int required_stable = 7; // Require 7 out of 8
                
                for (int i = 0; i < total_checks; i++)
                {
                    if (gpio_get_level(encoder->sw_pin) == 0)
                    {
                        stable_press_count++;
                    }
                    vTaskDelay(pdMS_TO_TICKS(15)); // Increased from 10ms
                }
                
                // Only trigger if button was consistently pressed
                if (stable_press_count >= required_stable)
                {
                    ESP_LOGI(TAG, "Button press confirmed (stable count: %d/%d)", stable_press_count, total_checks);
                    
                    if (encoder->on_button_press)
                    {
                        encoder->on_button_press(encoder);
                    }
                    
                    // Wait for button release to prevent multiple triggers
                    int release_count = 0;
                    int max_wait = 300; // 3 second timeout
                    while (release_count < 5 && max_wait > 0)
                    {
                        if (gpio_get_level(encoder->sw_pin) == 1)
                        {
                            release_count++;
                        }
                        else
                        {
                            release_count = 0; // Reset if goes back LOW
                        }
                        vTaskDelay(pdMS_TO_TICKS(10));
                        max_wait--;
                    }
                    
                    if (max_wait <= 0)
                    {
                        ESP_LOGW(TAG, "Button stuck LOW - possible hardware issue!");
                    }
                    
                    // Additional delay after release for debouncing
                    vTaskDelay(pdMS_TO_TICKS(300)); // Increased from 200ms
                }
                else
                {
                    ESP_LOGW(TAG, "Button press rejected - likely noise/bounce (stable count: %d/%d)", stable_press_count, total_checks);
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

    // Add debouncing for rotation - ignore rapid changes
    static uint32_t last_rotation_time = 0;
    uint32_t current_time = xTaskGetTickCountFromISR();
    
    // Ignore if less than 50ms since last rotation (debounce)
    if ((current_time - last_rotation_time) < pdMS_TO_TICKS(50))
    {
        return;
    }

    int clk_state = gpio_get_level(encoder->clk_pin);
    int dt_state = gpio_get_level(encoder->dt_pin);

    // Only trigger on CLK falling edge
    if (clk_state == 0 && encoder->last_clk_state == 1)
    {
        last_rotation_time = current_time;
        
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

    // Get current time
    static uint32_t last_button_time = 0;
    uint32_t current_time = xTaskGetTickCountFromISR();
    
    // Immediate check: verify button is actually LOW right now
    int button_state = gpio_get_level(encoder->sw_pin);
    if (button_state != 0)
    {
        // False trigger - button isn't even pressed
        return;
    }
    
    // Ignore if less than 500ms since last button press (increased from 200ms)
    if ((current_time - last_button_time) < pdMS_TO_TICKS(500))
    {
        return;
    }
    
    last_button_time = current_time;
    
    // Notify task to handle callback
    if (encoder->task_handle != NULL)
    {
        xTaskNotifyFromISR(encoder->task_handle, NOTIFY_BUTTON, eSetBits, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
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
    
    // Check initial button state for diagnostics
    int initial_button_state = gpio_get_level(self->sw_pin);
    ESP_LOGI(TAG, "Rotary encoder initialized successfully");
    ESP_LOGI(TAG, "  Button pin (GPIO%d) initial state: %d (0=pressed, 1=released)", self->sw_pin, initial_button_state);
    
    if (initial_button_state == 0)
    {
        ESP_LOGW(TAG, "  WARNING: Button appears to be pressed at startup! Check wiring or disable encoder in settings.");
    }

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
