#include "main_menu.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "led.h"
#include "settings_manager.h"

static const char *TAG = "MAIN_MENU";

#define MENU_OPTIONS 3
#define FADE_STEPS 20
#define FADE_STEP_MS 20
#define SELECTED_BRIGHTNESS 1.0f
#define DIMMED_BRIGHTNESS 0.05f  // Reduced from 0.2 to 0.05 for less bright unselected options

typedef struct {
    const char* name;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} menu_color_t;

static const menu_color_t menu_colors[MENU_OPTIONS] = {
    {"Flash",       255, 0,   0},      // Red - Opens sub-menu to toggle flash ON/OFF
    {"Self Timer",  0,   0,   255},    // Blue - Opens sub-menu to toggle timer ON/OFF
    {"Auto Print",  0,   255, 0}       // Green - Opens sub-menu to toggle auto-print ON/OFF
};

static LEDRing_t *g_led_ring = NULL;
static SettingsManager_t *g_settings = NULL;
static bool g_menu_visible = false;
static bool g_sub_menu_active = false;
static int g_current_menu_option = 0;
static TickType_t g_last_activity_time = 0;
static TimerHandle_t g_fade_timer = NULL;
static TaskHandle_t g_fade_task_handle = NULL;
static bool g_fade_out_requested = false;

#define FADE_OUT_TIMEOUT_MS 3000

// Forward declarations
static void fade_timer_callback(TimerHandle_t timer);
static void fade_task(void *pvParameters);

void main_menu_init(LEDRing_t *led_ring, SettingsManager_t *settings)
{
    g_led_ring = led_ring;
    g_settings = settings;
    g_menu_visible = false;
    g_sub_menu_active = false;
    g_current_menu_option = 0;
    g_last_activity_time = xTaskGetTickCount();
    g_fade_out_requested = false;
    
    // Create fade-out timer (one-shot, 3 seconds)
    g_fade_timer = xTimerCreate("FadeTimer", pdMS_TO_TICKS(FADE_OUT_TIMEOUT_MS), 
                                 pdFALSE, NULL, fade_timer_callback);
    
    // Create fade task to handle fade operations (larger stack)
    xTaskCreate(fade_task, "FadeTask", 4096, NULL, 5, &g_fade_task_handle);
    
    ESP_LOGI(TAG, "Main menu initialized with 3s fade-out timer");
}

void main_menu_fade_in(int selected_option)
{
    if (!g_led_ring || !g_settings)
        return;
    
    int led_count = g_settings->settings.led_ring_count;
    ESP_LOGI(TAG, "Fading in menu to option: %s", menu_colors[selected_option].name);
    
    for (int fade = 0; fade <= FADE_STEPS; fade++)
    {
        float fade_mult = (float)fade / FADE_STEPS;
        int leds_per_section = led_count / MENU_OPTIONS;
        int remainder = led_count % MENU_OPTIONS;
        int led_index = 0;
        
        for (int section = 0; section < MENU_OPTIONS; section++)
        {
            int section_size = leds_per_section + (section < remainder ? 1 : 0);
            bool is_selected = (section == selected_option);
            float brightness_mult = is_selected ? SELECTED_BRIGHTNESS : DIMMED_BRIGHTNESS;
            brightness_mult *= fade_mult;
            
            uint8_t r = (uint8_t)(menu_colors[section].r * brightness_mult);
            uint8_t g = (uint8_t)(menu_colors[section].g * brightness_mult);
            uint8_t b = (uint8_t)(menu_colors[section].b * brightness_mult);
            
            for (int i = 0; i < section_size; i++)
            {
                if (led_index < led_count)
                {
                    g_led_ring->set_pixel(g_led_ring, led_index, r, g, b);
                    led_index++;
                }
            }
        }
        
        g_led_ring->refresh(g_led_ring);
        vTaskDelay(pdMS_TO_TICKS(FADE_STEP_MS));
    }
    
    g_menu_visible = true;
    g_current_menu_option = selected_option;
    
    // Start fade-out timer
    main_menu_reset_timer();
}

void update_led_ring_menu(int selected_option)
{
    if (!g_led_ring || !g_settings)
        return;
    
    if (!g_menu_visible)
    {
        int led_count = g_settings->settings.led_ring_count;
        for (int i = 0; i < led_count; i++)
        {
            g_led_ring->set_pixel(g_led_ring, i, 0, 0, 0);
        }
        g_led_ring->refresh(g_led_ring);
        return;
    }
    
    int led_count = g_settings->settings.led_ring_count;
    int leds_per_section = led_count / MENU_OPTIONS;
    int remainder = led_count % MENU_OPTIONS;
    
    for (int i = 0; i < led_count; i++)
    {
        g_led_ring->set_pixel(g_led_ring, i, 0, 0, 0);
    }
    
    int led_index = 0;
    for (int section = 0; section < MENU_OPTIONS; section++)
    {
        int section_size = leds_per_section + (section < remainder ? 1 : 0);
        bool is_selected = (section == selected_option);
        float brightness_mult = is_selected ? SELECTED_BRIGHTNESS : DIMMED_BRIGHTNESS;
        
        uint8_t r = (uint8_t)(menu_colors[section].r * brightness_mult);
        uint8_t g = (uint8_t)(menu_colors[section].g * brightness_mult);
        uint8_t b = (uint8_t)(menu_colors[section].b * brightness_mult);
        
        for (int i = 0; i < section_size; i++)
        {
            if (led_index < led_count)
            {
                g_led_ring->set_pixel(g_led_ring, led_index, r, g, b);
                led_index++;
            }
        }
    }
    
    g_led_ring->refresh(g_led_ring);
    g_current_menu_option = selected_option;
    
    // Reset fade-out timer on menu update
    main_menu_reset_timer();
}

int get_current_menu_option(void)
{
    return g_current_menu_option;
}

void set_current_menu_option(int option)
{
    if (option >= 0 && option < MENU_OPTIONS)
    {
        g_current_menu_option = option;
    }
}

void refresh_led_ring_menu(void)
{
    update_led_ring_menu(g_current_menu_option);
}

bool is_menu_visible(void)
{
    return g_menu_visible;
}

const char* get_menu_option_name(int option)
{
    if (option >= 0 && option < MENU_OPTIONS)
    {
        return menu_colors[option].name;
    }
    return "Unknown";
}

int get_menu_options_count(void)
{
    return MENU_OPTIONS;
}

static void fade_timer_callback(TimerHandle_t timer)
{
    // Don't do heavy work in timer callback - just set a flag
    g_fade_out_requested = true;
}

static void fade_task(void *pvParameters)
{
    while (1)
    {
        // Wait for fade-out request
        if (g_fade_out_requested)
        {
            g_fade_out_requested = false;
            main_menu_fade_out();
        }
        
        // Check every 100ms
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void main_menu_fade_out(void)
{
    if (!g_led_ring || !g_settings || !g_menu_visible)
        return;
    
    int led_count = g_settings->settings.led_ring_count;
    ESP_LOGI(TAG, "Fading out menu after inactivity");
    
    // Fade out over FADE_STEPS
    for (int fade = FADE_STEPS; fade >= 0; fade--)
    {
        float fade_mult = (float)fade / FADE_STEPS;
        int leds_per_section = led_count / MENU_OPTIONS;
        int remainder = led_count % MENU_OPTIONS;
        int led_index = 0;
        
        for (int section = 0; section < MENU_OPTIONS; section++)
        {
            int section_size = leds_per_section + (section < remainder ? 1 : 0);
            bool is_selected = (section == g_current_menu_option);
            float brightness_mult = is_selected ? SELECTED_BRIGHTNESS : DIMMED_BRIGHTNESS;
            brightness_mult *= fade_mult;
            
            uint8_t r = (uint8_t)(menu_colors[section].r * brightness_mult);
            uint8_t g = (uint8_t)(menu_colors[section].g * brightness_mult);
            uint8_t b = (uint8_t)(menu_colors[section].b * brightness_mult);
            
            for (int i = 0; i < section_size; i++)
            {
                if (led_index < led_count)
                {
                    g_led_ring->set_pixel(g_led_ring, led_index, r, g, b);
                    led_index++;
                }
            }
        }
        
        g_led_ring->refresh(g_led_ring);
        vTaskDelay(pdMS_TO_TICKS(FADE_STEP_MS));
    }
    
    g_menu_visible = false;
}

void main_menu_reset_timer(void)
{
    g_last_activity_time = xTaskGetTickCount();
    
    if (g_fade_timer)
    {
        xTimerReset(g_fade_timer, 0);
    }
}

bool is_sub_menu_active(void)
{
    return g_sub_menu_active;
}

void main_menu_enter_sub_menu(bool initial_selection)
{
    if (!g_led_ring || !g_settings)
        return;
    
    ESP_LOGI(TAG, "Entering sub-menu (Self-Timer toggle)");
    g_sub_menu_active = true;
    
    // Stop fade-out timer during sub-menu
    main_menu_stop_timer();
    
    // Display the sub-menu immediately
    main_menu_update_sub_menu(initial_selection);
}

void main_menu_update_sub_menu(bool selection)
{
    if (!g_led_ring || !g_settings)
        return;
    
    int led_count = g_settings->settings.led_ring_count;
    int half = led_count / 2;
    
    // First half: RED (OFF)
    // Second half: GREEN (ON)
    for (int i = 0; i < led_count; i++)
    {
        if (i < half)
        {
            // Red side (OFF) - bright if selected, dim if not
            float brightness = selection ? DIMMED_BRIGHTNESS : SELECTED_BRIGHTNESS;
            uint8_t r = (uint8_t)(255 * brightness);
            g_led_ring->set_pixel(g_led_ring, i, r, 0, 0);
        }
        else
        {
            // Green side (ON) - bright if selected, dim if not
            float brightness = selection ? SELECTED_BRIGHTNESS : DIMMED_BRIGHTNESS;
            uint8_t g = (uint8_t)(255 * brightness);
            g_led_ring->set_pixel(g_led_ring, i, 0, g, 0);
        }
    }
    
    g_led_ring->refresh(g_led_ring);
    ESP_LOGI(TAG, "Sub-menu updated: Self-Timer %s", selection ? "ON" : "OFF");
}

void main_menu_exit_sub_menu(void)
{
    if (!g_led_ring || !g_settings)
        return;
    
    ESP_LOGI(TAG, "Exiting sub-menu, fading out...");
    g_sub_menu_active = false;
    g_menu_visible = false;
    
    // Fade out the LEDs
    int led_count = g_settings->settings.led_ring_count;
    int half = led_count / 2;
    
    for (int fade = FADE_STEPS; fade >= 0; fade--)
    {
        float fade_mult = (float)fade / FADE_STEPS;
        
        for (int i = 0; i < led_count; i++)
        {
            if (i < half)
            {
                // Red side
                uint8_t r = (uint8_t)(255 * DIMMED_BRIGHTNESS * fade_mult);
                g_led_ring->set_pixel(g_led_ring, i, r, 0, 0);
            }
            else
            {
                // Green side
                uint8_t g = (uint8_t)(255 * DIMMED_BRIGHTNESS * fade_mult);
                g_led_ring->set_pixel(g_led_ring, i, 0, g, 0);
            }
        }
        
        g_led_ring->refresh(g_led_ring);
        vTaskDelay(pdMS_TO_TICKS(FADE_STEP_MS));
    }
    
    // Turn off all LEDs
    for (int i = 0; i < led_count; i++)
    {
        g_led_ring->set_pixel(g_led_ring, i, 0, 0, 0);
    }
    g_led_ring->refresh(g_led_ring);
}

void main_menu_stop_timer(void)
{
    if (g_fade_timer)
    {
        xTimerStop(g_fade_timer, 0);
    }
    g_fade_out_requested = false;
}