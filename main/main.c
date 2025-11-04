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

// Menu configuration
#define CLICKS_PER_OPTION 4

// Rotary encoder rotation callback
void on_rotary_rotation(RotaryEncoder_t *encoder, int position)
{
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

// Button press callback - take a picture
void on_button_press(RotaryEncoder_t *encoder)
{
    ESP_LOGI(TAG, "Button pressed! Taking picture...");

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

            // Upload to server
            esp_err_t result = g_http_client->upload_image(g_http_client, fb);
            if (result == ESP_OK)
            {
                ESP_LOGI(TAG, "Picture uploaded successfully!");
            }
            else
            {
                ESP_LOGE(TAG, "Failed to upload picture");
            }

            g_camera->return_frame(g_camera, fb);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to capture picture");
        }
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
        
        // Initialize LED ring with all LEDs off (will turn on when encoder is rotated)
        led_ring->init(led_ring);
        led_ring->set_brightness(led_ring, settings->settings.led_ring_brightness);
        led_ring->clear(led_ring);
        led_ring->refresh(led_ring);
        ESP_LOGI(TAG, "LED Ring initialized - LEDs off until encoder rotation");
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

        // Wait for credentials (5 minutes timeout)
        if (!provisioning->wait_for_credentials(provisioning, 300000))
        {
            ESP_LOGE(TAG, "Provisioning timeout! Restarting...");
            wifi_provisioning_destroy(provisioning);
            settings_manager_destroy(settings);
            camera_destroy(camera);
            led_ring_destroy(led_ring);
            led_destroy(led);
            vTaskDelay(3000 / portTICK_PERIOD_MS);
            esp_restart();
            return;
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
    // Initialize LED Ring (if enabled)
    // ========================================================================
    if (led_ring)
    {
        ESP_LOGI(TAG, "\n--- Initializing LED Ring ---");
        if (led_ring->init(led_ring) == ESP_OK)
        {
            ESP_LOGI(TAG, "LED Ring initialized successfully");
            // Set brightness from settings
            led_ring->set_brightness(led_ring, settings->settings.led_ring_brightness);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize LED Ring!");
        }
    }

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
