#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_log.h"

// Include our custom "classes"
#include "led.h"
#include "camera.h"
#include "wifi_manager.h"
#include "wifi_provisioning.h"
#include "settings_manager.h"
#include "http_client.h"
#include "remote_control.h"

static const char *TAG = "MAIN";

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
            led_destroy(led);
            return;
        }

        // Start web portal
        if (provisioning->start_portal(provisioning) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to start portal!");
            wifi_provisioning_destroy(provisioning);
            camera_destroy(camera);
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
        led_destroy(led);
        return;
    }

    // Create Remote Control for polling server commands
    RemoteControl_t *remote_control = remote_control_create(settings, http_client);
    if (!remote_control)
    {
        ESP_LOGE(TAG, "Failed to create Remote Control object!");
        http_client_destroy(http_client);
        settings_manager_destroy(settings);
        wifi_destroy(wifi);
        camera_destroy(camera);
        led_destroy(led);
        return;
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
    remote_control_destroy(remote_control);
    http_client_destroy(http_client);
    wifi_destroy(wifi);
    camera_destroy(camera);
    led_destroy(led);
    settings_manager_destroy(settings);
}
