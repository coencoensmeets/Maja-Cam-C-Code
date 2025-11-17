#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_log.h"

// Include our custom "classes"
#include "led.h"
#include "led_ring.h"
#include "camera.h"
#include "wifi_manager.h"
#include "wifi_provisioning.h"
#include "settings_manager.h"
#include "http_client.h"
#include "remote_control.h"
#include "rotary_encoder.h"
#include "main_menu.h"
#include "thermal_printer.h"

static const char *TAG = "MAIN";

// Global pointers for callbacks
static Camera_t *g_camera = NULL;
static HttpClient_t *g_http_client = NULL;
static ThermalPrinter_t *g_thermal_printer = NULL;
static LEDRing_t *g_led_ring = NULL;
static SettingsManager_t *g_settings = NULL;

// Sub-menu state
static bool g_sub_menu_selection = false; // false = OFF (red), true = ON (green)
static int g_last_encoder_position = 0;   // Track last encoder position for relative changes
static int g_sub_menu_position_accumulator = 0; // Accumulate encoder changes in sub-menu
static int g_sub_menu_option = -1;        // Track which menu option opened the sub-menu (0=flash, 1=self-timer)

// WiFi provisioning pulse state
static bool g_stop_provisioning_pulse = false;
static TaskHandle_t g_provisioning_pulse_task = NULL;

// Startup animation state
static bool g_stop_startup_animation = false;
static TaskHandle_t g_startup_animation_task = NULL;

// Poem loading animation state
static bool g_stop_poem_loading_animation = false;
static TaskHandle_t g_poem_loading_animation_task = NULL;

// Menu configuration
#define CLICKS_PER_OPTION 2  // Changed from 4 to 2 for better response
#define SUB_MENU_CLICKS_TO_TOGGLE 3  // Require 3 ticks in sub-menu to toggle

// Forward declarations
void stop_poem_loading_animation(void);

// LED Animation functions
static void led_ring_startup_animation(LEDRing_t *led_ring, bool *stop_flag)
{
    if (!led_ring) return;
    
    ESP_LOGI(TAG, "Starting white pixel startup animation (continuous until stopped)...");
    int led_count = led_ring->num_leds;
    
    // Keep running until stop flag is set
    while (!(*stop_flag))
    {
        for (int i = 0; i < led_count; i++)
        {
            if (*stop_flag) break;
            
            // Clear all LEDs
            for (int j = 0; j < led_count; j++)
            {
                led_ring->set_pixel(led_ring, j, 0, 0, 0);
            }
            
            // Light current LED white
            led_ring->set_pixel(led_ring, i, 255, 255, 255);
            led_ring->refresh(led_ring);
            vTaskDelay(pdMS_TO_TICKS(25)); // 25ms per LED
        }
    }
    
    // Clear all LEDs at end
    led_ring->clear(led_ring);
    led_ring->refresh(led_ring);
    ESP_LOGI(TAG, "Startup animation stopped");
}

// Task wrapper for startup animation (runs in separate task)
static void startup_animation_task(void *pvParameters)
{
    LEDRing_t *led_ring = (LEDRing_t *)pvParameters;
    led_ring_startup_animation(led_ring, &g_stop_startup_animation);
    vTaskDelete(NULL);
}

// Magenta pixel loading animation (for poem generation)
static void led_ring_poem_loading_animation(LEDRing_t *led_ring, bool *stop_flag)
{
    if (!led_ring) return;
    
    ESP_LOGI(TAG, "Starting magenta pixel poem loading animation...");
    int led_count = led_ring->num_leds;
    
    // Keep running until stop flag is set
    while (!(*stop_flag))
    {
        for (int i = 0; i < led_count; i++)
        {
            if (*stop_flag) break;
            
            // Clear all LEDs
            for (int j = 0; j < led_count; j++)
            {
                led_ring->set_pixel(led_ring, j, 0, 0, 0);
            }
            
            // Light current LED magenta (255, 0, 255)
            led_ring->set_pixel(led_ring, i, 255, 0, 255);
            led_ring->refresh(led_ring);
            vTaskDelay(pdMS_TO_TICKS(25)); // 25ms per LED
        }
    }
    
    // Clear all LEDs at end
    led_ring->clear(led_ring);
    led_ring->refresh(led_ring);
    ESP_LOGI(TAG, "Poem loading animation stopped");
}

// Task wrapper for poem loading animation (runs in separate task)
static void poem_loading_animation_task(void *pvParameters)
{
    LEDRing_t *led_ring = (LEDRing_t *)pvParameters;
    led_ring_poem_loading_animation(led_ring, &g_stop_poem_loading_animation);
    vTaskDelete(NULL);
}

static void led_ring_wifi_connected_flash(LEDRing_t *led_ring)
{
    if (!led_ring) return;
    
    ESP_LOGI(TAG, "WiFi connected! Green pulse flash...");
    int led_count = led_ring->num_leds;
    
    // Smooth fade in green (1.5 seconds - increased from 510ms)
    for (int brightness = 0; brightness <= 255; brightness += 3)
    {
        for (int i = 0; i < led_count; i++)
        {
            led_ring->set_pixel(led_ring, i, 0, brightness, 0);
        }
        led_ring->refresh(led_ring);
        vTaskDelay(pdMS_TO_TICKS(18)); // 18ms * 85 steps = 1530ms fade in
    }
    
    // Hold at full brightness longer (1 second - increased from 200ms)
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Smooth fade out (1.5 seconds - increased from 306ms)
    for (int brightness = 255; brightness >= 0; brightness -= 3)
    {
        for (int i = 0; i < led_count; i++)
        {
            led_ring->set_pixel(led_ring, i, 0, brightness, 0);
        }
        led_ring->refresh(led_ring);
        vTaskDelay(pdMS_TO_TICKS(18)); // 18ms * 85 steps = 1530ms fade out
    }
    
    // Clear all LEDs
    led_ring->clear(led_ring);
    led_ring->refresh(led_ring);
}

static void led_ring_wifi_provisioning_pulse(LEDRing_t *led_ring, bool *stop_flag)
{
    if (!led_ring) return;
    
    ESP_LOGI(TAG, "Starting WiFi provisioning blue pulse...");
    int led_count = led_ring->num_leds;
    
    while (!(*stop_flag))
    {
        // Breathe in (2 seconds - increased from 1 second)
        for (int brightness = 0; brightness <= 255; brightness += 2)
        {
            if (*stop_flag) break;
            
            for (int i = 0; i < led_count; i++)
            {
                led_ring->set_pixel(led_ring, i, 0, 0, brightness);
            }
            led_ring->refresh(led_ring);
            vTaskDelay(pdMS_TO_TICKS(16)); // 16ms * 128 steps = 2048ms (2 seconds)
        }
        
        // Breathe out (2 seconds - increased from 1 second)
        for (int brightness = 255; brightness >= 0; brightness -= 2)
        {
            if (*stop_flag) break;
            
            for (int i = 0; i < led_count; i++)
            {
                led_ring->set_pixel(led_ring, i, 0, 0, brightness);
            }
            led_ring->refresh(led_ring);
            vTaskDelay(pdMS_TO_TICKS(16)); // 16ms * 128 steps = 2048ms (2 seconds)
        }
    }
    
    // Clear all LEDs when stopped
    led_ring->clear(led_ring);
    led_ring->refresh(led_ring);
}

// Task wrapper for provisioning pulse (runs in separate task)
static void provisioning_pulse_task(void *pvParameters)
{
    LEDRing_t *led_ring = (LEDRing_t *)pvParameters;
    led_ring_wifi_provisioning_pulse(led_ring, &g_stop_provisioning_pulse);
    vTaskDelete(NULL);
}

// Rotary encoder rotation callback
void on_rotary_rotation(RotaryEncoder_t *encoder, int position)
{
    // Check if we're in sub-menu mode
    if (is_sub_menu_active())
    {
        // Accumulate encoder changes - require SUB_MENU_CLICKS_TO_TOGGLE ticks to toggle
        int position_delta = position - g_last_encoder_position;
        
        if (position_delta != 0)
        {
            g_sub_menu_position_accumulator += position_delta;
            g_last_encoder_position = position;
            
            // Check if we've accumulated enough ticks to toggle
            if (abs(g_sub_menu_position_accumulator) >= SUB_MENU_CLICKS_TO_TOGGLE)
            {
                g_sub_menu_selection = !g_sub_menu_selection;
                g_sub_menu_position_accumulator = 0; // Reset accumulator after toggle
                ESP_LOGI(TAG, "Sub-menu selection toggled: Self-Timer %s", g_sub_menu_selection ? "ON" : "OFF");
                main_menu_update_sub_menu(g_sub_menu_selection);
            }
        }
        return;
    }
    
    // Main menu rotation handling
    // Calculate menu option based on encoder position
    int new_option = ((position / CLICKS_PER_OPTION) % get_menu_options_count() + get_menu_options_count()) % get_menu_options_count();
    
    // If this is the first rotation, fade in the menu
    if (!is_menu_visible())
    {
        ESP_LOGI(TAG, "First rotation - fading in menu: %s", get_menu_option_name(new_option));
        main_menu_fade_in(new_option);
        return;
    }
    
    // Only update if menu option changed
    if (new_option != get_current_menu_option())
    {
        ESP_LOGI(TAG, "Menu: %s (Option %d)", get_menu_option_name(new_option), new_option);
        update_led_ring_menu(new_option);
    }
    else
    {
        // Even if option didn't change, reset the fade-out timer
        main_menu_reset_timer();
    }
}

// Button press callback - take a picture or handle menu selection
void on_button_press(RotaryEncoder_t *encoder)
{
    ESP_LOGI(TAG, "Button pressed!");
    
    // Check if we're in sub-menu mode
    if (is_sub_menu_active())
    {
        // Save the appropriate setting based on which option opened the sub-menu
        if (g_settings)
        {
            if (g_sub_menu_option == 0)
            {
                // Flash setting
                g_settings->settings.flash_enabled = g_sub_menu_selection;
                g_settings->save_settings(g_settings);
                ESP_LOGI(TAG, "Flash setting saved: %s", g_sub_menu_selection ? "ENABLED" : "DISABLED");
            }
            else if (g_sub_menu_option == 1)
            {
                // Self-timer setting
                g_settings->settings.self_timer_enabled = g_sub_menu_selection;
                g_settings->save_settings(g_settings);
                ESP_LOGI(TAG, "Self-timer setting saved: %s", g_sub_menu_selection ? "ENABLED" : "DISABLED");
            }
            else if (g_sub_menu_option == 2)
            {
                // Auto-print setting
                g_settings->settings.auto_print_enabled = g_sub_menu_selection;
                g_settings->save_settings(g_settings);
                ESP_LOGI(TAG, "Auto-print setting saved: %s", g_sub_menu_selection ? "ENABLED" : "DISABLED");
            }
        }
        
        // Exit sub-menu (fade out and return to main mode)
        main_menu_exit_sub_menu();
        g_sub_menu_option = -1; // Reset
        return;
    }
    
    // Check if main menu is visible
    if (is_menu_visible())
    {
        int selected_option = get_current_menu_option();
        ESP_LOGI(TAG, "Menu option %d selected: %s", selected_option, get_menu_option_name(selected_option));
        
        // Option 0 (Red - Flash Settings)
        if (selected_option == 0)
        {
            ESP_LOGI(TAG, "Opening Flash settings sub-menu...");
            
            // Get current setting
            bool current_setting = true; // Default to enabled
            if (g_settings)
            {
                current_setting = g_settings->settings.flash_enabled;
            }
            
            // Enter sub-menu with current setting as initial selection
            g_sub_menu_selection = current_setting;
            g_sub_menu_option = 0; // Track that we're editing flash
            g_last_encoder_position = encoder->get_position(encoder);
            g_sub_menu_position_accumulator = 0;
            main_menu_enter_sub_menu(current_setting);
            return;
        }
        
        // Option 1 (Blue - Self Timer Settings)
        if (selected_option == 1)
        {
            ESP_LOGI(TAG, "Opening Self-Timer settings sub-menu...");
            
            // Get current setting
            bool current_setting = false;
            if (g_settings)
            {
                current_setting = g_settings->settings.self_timer_enabled;
            }
            
            // Enter sub-menu with current setting as initial selection
            g_sub_menu_selection = current_setting;
            g_sub_menu_option = 1; // Track that we're editing self-timer
            g_last_encoder_position = encoder->get_position(encoder); // Save current position
            g_sub_menu_position_accumulator = 0; // Reset accumulator for sub-menu
            main_menu_enter_sub_menu(current_setting);
            return;
        }
        
        // Option 2 (Green - Auto Print Settings)
        if (selected_option == 2)
        {
            ESP_LOGI(TAG, "Opening Auto Print settings sub-menu...");
            
            // Get current setting
            bool current_setting = false;
            if (g_settings)
            {
                current_setting = g_settings->settings.auto_print_enabled;
            }
            
            // Enter sub-menu with current setting as initial selection
            g_sub_menu_selection = current_setting;
            g_sub_menu_option = 2; // Track that we're editing auto-print
            g_last_encoder_position = encoder->get_position(encoder);
            g_sub_menu_position_accumulator = 0;
            main_menu_enter_sub_menu(current_setting);
            return;
        }
        
        // For other menu options, just fade out and take picture
        ESP_LOGI(TAG, "Other menu option - taking picture");
        main_menu_stop_timer();
    }
    else
    {
        ESP_LOGI(TAG, "Menu not visible - taking picture");
    }
    
    // Take picture with or without self-timer based on setting
    bool use_self_timer = false;
    if (g_settings)
    {
        use_self_timer = g_settings->settings.self_timer_enabled;
    }
    
    if (use_self_timer && g_led_ring)
    {
        ESP_LOGI(TAG, "Starting 5-second self-timer countdown...");
        
        int led_count = g_led_ring->num_leds;
        
        // Do 5 complete rounds around the ring (1 second per round)
        // Delay per LED = 1000ms / 40 LEDs = 25ms per LED
        int delay_per_led = 1000 / led_count;
        
        for (int round = 0; round < 5; round++)
        {
            // Calculate color for this round (fade red to green across the 5 rounds)
            uint8_t red = 255 - ((255 * round) / 5);
            uint8_t green = (255 * round) / 5;
            
            // One complete circle around the ring
            for (int i = 0; i < led_count; i++)
            {
                // Only light the current LED, turn off all others
                for (int j = 0; j < led_count; j++)
                {
                    if (j == i)
                    {
                        // Current LED - color for this round
                        g_led_ring->set_pixel(g_led_ring, j, red, green, 0);
                    }
                    else
                    {
                        // All other LEDs - off
                        g_led_ring->set_pixel(g_led_ring, j, 0, 0, 0);
                    }
                }
                
                g_led_ring->refresh(g_led_ring);
                vTaskDelay(pdMS_TO_TICKS(delay_per_led));
            }
        }
        
        // Flash bright white if flash is enabled (500ms)
        bool flash_enabled = true; // Default to enabled
        if (g_settings)
        {
            flash_enabled = g_settings->settings.flash_enabled;
        }
        
        if (flash_enabled)
        {
            for (int i = 0; i < led_count; i++)
            {
                g_led_ring->set_pixel(g_led_ring, i, 255, 255, 255);
            }
            g_led_ring->refresh(g_led_ring);
            vTaskDelay(pdMS_TO_TICKS(500)); // Keep flash on for 500ms
        }
    }
    else
    {
        // No self-timer, but check if we should flash
        bool flash_enabled = true; // Default to enabled
        if (g_settings)
        {
            flash_enabled = g_settings->settings.flash_enabled;
        }
        
        if (flash_enabled && g_led_ring)
        {
            int led_count = g_led_ring->num_leds;
            for (int i = 0; i < led_count; i++)
            {
                g_led_ring->set_pixel(g_led_ring, i, 255, 255, 255);
            }
            g_led_ring->refresh(g_led_ring);
            vTaskDelay(pdMS_TO_TICKS(500)); // Keep flash on for 500ms
            
            // Turn off the flash
            g_led_ring->clear(g_led_ring);
            g_led_ring->refresh(g_led_ring);
            vTaskDelay(pdMS_TO_TICKS(50)); // Ensure RMT channel is released
        }
    }
    
    // Take the picture
    if (g_camera && g_http_client)
    {
        // Capture and immediately discard one frame to ensure we get fresh data
        camera_fb_t *flush_fb = g_camera->capture(g_camera);
        if (flush_fb)
        {
            g_camera->return_frame(g_camera, flush_fb);
        }
        
        // Small delay to let sensor update
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Now capture the actual image we want
        camera_fb_t *fb = g_camera->capture(g_camera);
        if (fb)
        {
            ESP_LOGI(TAG, "Picture captured: %zu bytes", fb->len);

            // Check if auto-print is enabled
            bool auto_print = false;
            if (g_settings)
            {
                auto_print = g_settings->settings.auto_print_enabled;
            }

            // Start magenta loading animation if auto-print is enabled
            if (auto_print && g_led_ring)
            {
                ESP_LOGI(TAG, "Auto-print enabled - starting poem loading animation");
                
                // Ensure LEDs are completely off before starting new animation
                g_led_ring->clear(g_led_ring);
                g_led_ring->refresh(g_led_ring);
                vTaskDelay(pdMS_TO_TICKS(50)); // Small delay to ensure RMT channel is released
                
                g_stop_poem_loading_animation = false;
                xTaskCreate(poem_loading_animation_task, "PoemLoadAnim", 4096, (void *)g_led_ring, 5, &g_poem_loading_animation_task);
            }

            // Upload to server
            esp_err_t result = g_http_client->upload_image(g_http_client, fb);
            if (result == ESP_OK)
            {
                ESP_LOGI(TAG, "Picture uploaded successfully!");
            }
            else
            {
                ESP_LOGE(TAG, "Failed to upload picture");
                // If upload failed and auto-print is on, stop the animation
                if (auto_print)
                {
                    stop_poem_loading_animation();
                }
            }

            g_camera->return_frame(g_camera, fb);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to capture picture");
        }
    }
    
    // Turn off LEDs after picture
    if (g_led_ring)
    {
        int led_count = g_led_ring->num_leds;
        for (int i = 0; i < led_count; i++)
        {
            g_led_ring->set_pixel(g_led_ring, i, 0, 0, 0);
        }
        g_led_ring->refresh(g_led_ring);
    }
}

// Function to stop poem loading animation (can be called from other modules)
void stop_poem_loading_animation(void)
{
    if (g_poem_loading_animation_task != NULL)
    {
        ESP_LOGI(TAG, "Stopping poem loading animation (called externally)");
        g_stop_poem_loading_animation = true;
        vTaskDelay(pdMS_TO_TICKS(100)); // Give task time to finish
        g_poem_loading_animation_task = NULL;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "  ESP32-S3 Camera Web Server");
    ESP_LOGI(TAG, "  Object-Oriented Architecture");
    ESP_LOGI(TAG, "===========================================");

    // Uncomment the line below to clear stored WiFi credentials (force re-provisioning)
    // wifi_credentials_clear();

    // Print chip info
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "\nESP32-S3 Chip:");
    ESP_LOGI(TAG, "  Cores: %d", chip_info.cores);
    ESP_LOGI(TAG, "  Model: %d", chip_info.model);
    ESP_LOGI(TAG, "  Revision: %d", chip_info.revision);
    ESP_LOGI(TAG, "  Free heap: %lu bytes", esp_get_free_heap_size());

    // ========================================================================
    // Create all objects
    // ========================================================================
    ESP_LOGI(TAG, "\n--- Creating Objects ---");

    LED_t *led = led_create(GPIO_NUM_2);
    if (!led)
    {
        ESP_LOGE(TAG, "Failed to create LED object!");
        return;
    }

    Camera_t *camera = camera_create(led);
    if (!camera)
    {
        ESP_LOGE(TAG, "Failed to create Camera object!");
        led_destroy(led);
        return;
    }

    // ========================================================================
    // Initialize Settings Manager
    // ========================================================================
    ESP_LOGI(TAG, "\n--- Initializing Settings Manager ---");
    SettingsManager_t *settings = settings_manager_create();
    if (!settings)
    {
        ESP_LOGE(TAG, "Failed to create Settings Manager!");
        camera_destroy(camera);
        led_destroy(led);
        return;
    }

    if (settings->init(settings) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize Settings Manager!");
        settings_manager_destroy(settings);
        camera_destroy(camera);
        led_destroy(led);
        return;
    }

    // Store settings globally for callback access
    g_settings = settings;

    // Print current settings
    settings->print(settings);

    // ========================================================================
    // Create LED Ring with settings
    // ========================================================================
    ESP_LOGI(TAG, "\n--- Creating LED Ring ---");
    
    LEDRing_t *led_ring = NULL;
    if (settings->settings.led_ring_enabled)
    {
        ESP_LOGI(TAG, "LED Ring config: GPIO%d, Count=%d", 
                 settings->settings.led_ring_data_pin, settings->settings.led_ring_count);
        
        led_ring = led_ring_create((gpio_num_t)settings->settings.led_ring_data_pin, 
                                    settings->settings.led_ring_count);
        if (!led_ring)
        {
            ESP_LOGE(TAG, "Failed to create LED Ring object!");
            settings_manager_destroy(settings);
            camera_destroy(camera);
            led_destroy(led);
            return;
        }
        
        // Initialize LED ring
        led_ring->init(led_ring);
        led_ring->set_brightness(led_ring, settings->settings.led_ring_brightness);
        
        // Start startup animation in background task (non-blocking)
        g_stop_startup_animation = false;
        xTaskCreate(startup_animation_task, "StartupAnim", 4096, (void *)led_ring, 5, &g_startup_animation_task);
        
        ESP_LOGI(TAG, "LED Ring initialized - startup animation running in background");
        
        // Assign to global for button callback access
        g_led_ring = led_ring;
    }
    else
    {
        ESP_LOGI(TAG, "LED Ring disabled in settings");
    }
    // Initialize main menu system
    main_menu_init(led_ring, settings);

    // ========================================================================
    // Check WiFi Provisioning
    // ========================================================================
    ESP_LOGI(TAG, "\n--- Checking WiFi Provisioning ---");

    WiFi_t *wifi = NULL;

    if (!settings->has_wifi_credentials(settings))
    {
        ESP_LOGI(TAG, "No WiFi credentials found. Starting provisioning mode...");

        // Create provisioning manager
        WiFiProvisioning_t *provisioning = wifi_provisioning_create(led);
        if (!provisioning)
        {
            ESP_LOGE(TAG, "Failed to create provisioning object!");
            camera_destroy(camera);
            led_ring_destroy(led_ring);
            led_destroy(led);
            return;
        }

        // Initialize provisioning
        provisioning->init(provisioning);

        // Start Access Point
        if (provisioning->start_ap(provisioning) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to start AP!");
            wifi_provisioning_destroy(provisioning);
            camera_destroy(camera);
            led_ring_destroy(led_ring);
            led_destroy(led);
            return;
        }

        // Start web portal
        if (provisioning->start_portal(provisioning) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to start portal!");
            wifi_provisioning_destroy(provisioning);
            camera_destroy(camera);
            led_ring_destroy(led_ring);
            led_destroy(led);
            return;
        }

        ESP_LOGI(TAG, "\n===========================================");
        ESP_LOGI(TAG, "  PROVISIONING MODE");
        ESP_LOGI(TAG, "===========================================");
        ESP_LOGI(TAG, "  1. Connect to WiFi: %s", "ESP32-Camera-Setup");
        ESP_LOGI(TAG, "  2. Open browser: http://192.168.4.1");
        ESP_LOGI(TAG, "  3. Enter your WiFi credentials");
        ESP_LOGI(TAG, "===========================================\n");

        // Stop startup animation and start blue pulsing animation on LED ring during provisioning
        if (led_ring)
        {
            // Stop startup animation if still running
            g_stop_startup_animation = true;
            vTaskDelay(pdMS_TO_TICKS(100)); // Give startup animation time to clean up
            
            // Start provisioning pulse
            g_stop_provisioning_pulse = false;
            xTaskCreate(provisioning_pulse_task, "ProvisionPulse", 4096, (void *)led_ring, 5, &g_provisioning_pulse_task);
        }

        // Wait for credentials (5 minutes timeout)
        if (!provisioning->wait_for_credentials(provisioning, 300000))
        {
            ESP_LOGE(TAG, "Provisioning timeout! Restarting...");
            
            // Stop pulsing animation
            if (led_ring)
            {
                g_stop_provisioning_pulse = true;
                vTaskDelay(pdMS_TO_TICKS(100)); // Give task time to exit
            }
            
            wifi_provisioning_destroy(provisioning);
            settings_manager_destroy(settings);
            camera_destroy(camera);
            led_ring_destroy(led_ring);
            led_destroy(led);
            vTaskDelay(3000 / portTICK_PERIOD_MS);
            esp_restart();
            return;
        }

        // Stop pulsing animation when credentials received
        if (led_ring)
        {
            g_stop_provisioning_pulse = true;
            vTaskDelay(pdMS_TO_TICKS(100)); // Give task time to clean up
        }

        // Get credentials and save to settings
        wifi_credentials_t *creds = provisioning->get_credentials(provisioning);
        settings->set_wifi_credentials(settings, creds->ssid, creds->password);

        // Stop provisioning
        provisioning->stop(provisioning);
        wifi_provisioning_destroy(provisioning);

        // Wait a bit for AP to shut down
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "Provisioning complete! Connecting to WiFi...");
    }

    // Create WiFi manager with credentials from settings
    char ssid[32], password[64];
    if (settings->get_wifi_credentials(settings, ssid, password) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get WiFi credentials!");
        settings_manager_destroy(settings);
        camera_destroy(camera);
        led_ring_destroy(led_ring);
        led_destroy(led);
        return;
    }

    ESP_LOGI(TAG, "Using WiFi credentials: %s", ssid);
    wifi = wifi_create(ssid, password, led);

    if (!wifi)
    {
        ESP_LOGE(TAG, "Failed to create WiFi object!");
        settings_manager_destroy(settings);
        camera_destroy(camera);
        led_ring_destroy(led_ring);
        led_destroy(led);
        return;
    }

    // Create HTTP client for uploading to Flask server
    HttpClient_t *http_client = http_client_create(camera, settings);
    if (!http_client)
    {
        ESP_LOGE(TAG, "Failed to create HTTP Client object!");
        settings_manager_destroy(settings);
        wifi_destroy(wifi);
        camera_destroy(camera);
        led_ring_destroy(led_ring);
        led_destroy(led);
        return;
    }

    // ========================================================================
    // Create Thermal Printer
    // ========================================================================
    ESP_LOGI(TAG, "\n--- Creating Thermal Printer ---");
    
    if (settings->settings.printer_enabled)
    {
        printer_config_t printer_config = {
            .uart_port = (uart_port_t)settings->settings.printer_uart_port,
            .tx_pin = (gpio_num_t)settings->settings.printer_tx_pin,
            .rx_pin = (gpio_num_t)settings->settings.printer_rx_pin,
            .rts_pin = (gpio_num_t)settings->settings.printer_rts_pin,
            .baud_rate = settings->settings.printer_baud_rate,
            .max_print_width = settings->settings.printer_max_width
        };
        
        ESP_LOGI(TAG, "Printer config from settings: UART%d, TX=%d, RX=%d, RTS=%d, Baud=%d",
                 printer_config.uart_port, printer_config.tx_pin, printer_config.rx_pin,
                 printer_config.rts_pin, printer_config.baud_rate);
        
        g_thermal_printer = thermal_printer_create(printer_config);
        if (!g_thermal_printer)
        {
            ESP_LOGE(TAG, "Failed to create Thermal Printer object!");
            // Continue without printer (non-critical)
        }
        else if (g_thermal_printer->init(g_thermal_printer) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize Thermal Printer!");
            thermal_printer_destroy(g_thermal_printer);
            g_thermal_printer = NULL;
        }
        else
        {
            ESP_LOGI(TAG, "Thermal Printer initialized successfully at %d baud", printer_config.baud_rate);
        }
    }
    else
    {
        ESP_LOGI(TAG, "Thermal Printer disabled in settings");
        g_thermal_printer = NULL;
    }

    // Create Remote Control for polling server commands
    RemoteControl_t *remote_control = remote_control_create(settings, http_client, led_ring, g_thermal_printer);
    if (!remote_control)
    {
        ESP_LOGE(TAG, "Failed to create Remote Control object!");
        if (g_thermal_printer) thermal_printer_destroy(g_thermal_printer);
        http_client_destroy(http_client);
        settings_manager_destroy(settings);
        wifi_destroy(wifi);
        camera_destroy(camera);
        led_ring_destroy(led_ring);
        led_destroy(led);
        return;
    }

    // Create Rotary Encoder with push button
    ESP_LOGI(TAG, "\n--- Creating Rotary Encoder ---");
    
    RotaryEncoder_t *rotary = NULL;
    if (settings->settings.encoder_enabled)
    {
        ESP_LOGI(TAG, "Encoder config: CLK=%d, DT=%d, SW=%d",
                 settings->settings.encoder_clk_pin,
                 settings->settings.encoder_dt_pin,
                 settings->settings.encoder_sw_pin);
        
        rotary = rotary_encoder_create((gpio_num_t)settings->settings.encoder_clk_pin,
                                       (gpio_num_t)settings->settings.encoder_dt_pin,
                                       (gpio_num_t)settings->settings.encoder_sw_pin);
        if (!rotary)
        {
            ESP_LOGE(TAG, "Failed to create Rotary Encoder object!");
            remote_control_destroy(remote_control);
            http_client_destroy(http_client);
            settings_manager_destroy(settings);
            wifi_destroy(wifi);
            camera_destroy(camera);
            if (led_ring) led_ring_destroy(led_ring);
            led_destroy(led);
            return;
        }
    }
    else
    {
        ESP_LOGI(TAG, "Rotary Encoder disabled in settings");
    }

    // ========================================================================
    // Initialize LED
    // ========================================================================
    ESP_LOGI(TAG, "\n--- Initializing LED ---");
    led->init(led);
    led->blink(led, 3); // Startup blink

    // ========================================================================
    // Initialize Camera
    // ========================================================================
    ESP_LOGI(TAG, "\n--- Initializing Camera ---");
    if (camera->init(camera) != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera initialization failed!");
        while (1)
        {
            led->blink(led, 10);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    // Set camera orientation (optional - uncomment to use)
    // camera->set_rotation(camera, 0);    // 0° (normal)
    // camera->set_rotation(camera, 90);   // 90° clockwise
    // camera->set_rotation(camera, 180);  // 180° upside down
    // camera->set_rotation(camera, 270);  // 270° (90° counter-clockwise)

    // Or set individual flip/mirror settings:
    // camera->set_hmirror(camera, 0);     // Horizontal mirror: 0=off, 1=on
    // camera->set_vflip(camera, 0);       // Vertical flip: 0=off, 1=on

    // Apply rotation from settings
    ESP_LOGI(TAG, "Applying camera rotation from settings: %d°", settings->settings.camera_rotation);
    camera->set_rotation(camera, settings->settings.camera_rotation);

    // ========================================================================
    // Initialize Rotary Encoder
    // ========================================================================
    ESP_LOGI(TAG, "\n--- Initializing Rotary Encoder ---");

    // Set global pointers for callbacks
    g_camera = camera;
    g_http_client = http_client;

    // Set callbacks and initialize rotary encoder (if enabled)
    if (rotary)
    {
        rotary_encoder_set_rotation_callback(rotary, on_rotary_rotation);
        rotary_encoder_set_button_callback(rotary, on_button_press);

        // Initialize rotary encoder
        if (rotary->init(rotary) == ESP_OK)
        {
            ESP_LOGI(TAG, "Rotary encoder initialized successfully");
            ESP_LOGI(TAG, "  Rotate: Navigate menu (5 options)");
            ESP_LOGI(TAG, "  Press button: Take & upload picture");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize rotary encoder!");
        }
    }

    // Test capture
    ESP_LOGI(TAG, "\n--- Testing Camera Capture ---");
    camera_fb_t *test_fb = camera->capture(camera);
    if (test_fb)
    {
        camera->print_info(camera, test_fb);
        camera->return_frame(camera, test_fb);
        ESP_LOGI(TAG, "Camera test successful!");
    }
    else
    {
        ESP_LOGE(TAG, "Camera test failed!");
    }

    // ========================================================================
    // Initialize WiFi
    // ========================================================================
    ESP_LOGI(TAG, "\n--- Initializing WiFi ---");
    wifi->init(wifi);

    // Wait for WiFi connection (will retry indefinitely)
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    ESP_LOGI(TAG, "This will retry until connected...");
    wifi->wait_for_connection_retry(wifi);

    // WiFi connected! Show green flash on LED ring
    if (led_ring)
    {
        // Stop startup animation if still running
        g_stop_startup_animation = true;
        vTaskDelay(pdMS_TO_TICKS(100)); // Give startup animation time to clean up
        
        // Show green flash
        led_ring_wifi_connected_flash(led_ring);
    }

    // Get IP address
    char *ip_address = wifi->get_ip_address(wifi);

    // ========================================================================
    // Initialize HTTP Client
    // ========================================================================
    ESP_LOGI(TAG, "\n--- Initializing HTTP Client ---");
    if (http_client->init(http_client) == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP Client initialized successfully");
        ESP_LOGI(TAG, "Auto-upload disabled - images only uploaded on trigger");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP Client!");
    }

    // ========================================================================
    // Initialize Remote Control (Polling)
    // ========================================================================
    ESP_LOGI(TAG, "\n--- Initializing Remote Control ---");
    if (remote_control->init(remote_control) == ESP_OK)
    {
        ESP_LOGI(TAG, "Starting remote control polling...");
        remote_control->start_polling(remote_control);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize Remote Control!");
    }

    // ========================================================================
    // System Ready
    // ========================================================================
    ESP_LOGI(TAG, "\n===========================================");
    ESP_LOGI(TAG, "  System Ready!");
    ESP_LOGI(TAG, "  Camera: %s", camera->initialized ? "Online" : "Offline");
    ESP_LOGI(TAG, "  WiFi: Connected");
    ESP_LOGI(TAG, "  IP Address: %s", ip_address);
    ESP_LOGI(TAG, "  Thermal Printer: %s", g_thermal_printer ? "Available" : "Not Connected");
    ESP_LOGI(TAG, "  Upload Mode: Trigger Only");
    ESP_LOGI(TAG, "  Remote Control: Polling every %lu ms", settings->settings.server_poll_interval);
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "  Flask server URL:");
    ESP_LOGI(TAG, "  %s", settings->settings.server_upload_url);
    ESP_LOGI(TAG, "===========================================\n");

    // ========================================================================
    // Main loop - keep application alive
    // ========================================================================
    ESP_LOGI(TAG, "Entering main loop...\n");

    // Log status every minute
    uint32_t loop_count = 0;
    while (1)
    {
        vTaskDelay(60000 / portTICK_PERIOD_MS); // Wait 60 seconds
        loop_count++;
        ESP_LOGI(TAG, "System running... (uptime: %lu minutes)", loop_count);
        ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    }

    // Cleanup (never reached, but good practice)
    if (g_thermal_printer) thermal_printer_destroy(g_thermal_printer);
    rotary_encoder_destroy(rotary);
    remote_control_destroy(remote_control);
    http_client_destroy(http_client);
    wifi_destroy(wifi);
    camera_destroy(camera);
    led_ring_destroy(led_ring);
    led_destroy(led);
    settings_manager_destroy(settings);
}
