#include "remote_control.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "main_menu.h"
#include "main.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "REMOTE_CONTROL";

// HTTP response buffer
#define MAX_HTTP_OUTPUT_BUFFER 2048

// Task handle for polling
static TaskHandle_t polling_task_handle = NULL;

// Polling task
static void polling_task(void *arg)
{
    RemoteControl_t *self = (RemoteControl_t *)arg;
    char response_buffer[MAX_HTTP_OUTPUT_BUFFER];

    ESP_LOGI(TAG, "Polling task started (interval: %d ms)", self->poll_interval_ms);

    while (self->running)
    {
        // Check for capture command
        char command_url[300];
        snprintf(command_url, sizeof(command_url), "%s/api/command", self->server_url);

        esp_http_client_config_t config = {
            .url = command_url,
            .method = HTTP_METHOD_GET,
            .timeout_ms = 5000,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client)
        {
            memset(response_buffer, 0, sizeof(response_buffer));
            esp_http_client_set_header(client, "Content-Type", "application/json");

            esp_err_t err = esp_http_client_open(client, 0);
            if (err == ESP_OK)
            {
                esp_http_client_fetch_headers(client);
                int data_read = esp_http_client_read_response(client, response_buffer, MAX_HTTP_OUTPUT_BUFFER - 1);

                if (data_read >= 0)
                {
                    response_buffer[data_read] = '\0';
                    int status_code = esp_http_client_get_status_code(client);

                    if (status_code == 200)
                    {
                        // Parse JSON response
                        cJSON *root = cJSON_Parse(response_buffer);
                        if (root)
                        {
                            cJSON *command = cJSON_GetObjectItem(root, "command");
                            if (command && cJSON_IsString(command))
                            {
                                // Only log non-"none" commands
                                if (strcmp(command->valuestring, "none") != 0)
                                {
                                    ESP_LOGI(TAG, "Command received: '%s'", command->valuestring);
                                }
                                
                                if (strcmp(command->valuestring, "capture") == 0)
                                {
                                    ESP_LOGI(TAG, "Capture command received from server");
                                    self->http_client->capture_and_upload(self->http_client);
                                }
                                else if (strcmp(command->valuestring, "print") == 0)
                                {
                                    ESP_LOGI(TAG, "Print command received from server");
                                    
                                    // Stop poem loading animation first (whether printer is available or not)
                                    stop_poem_loading_animation();
                                    
                                    // Check if printer is available
                                    if (self->printer && self->printer->initialized)
                                    {
                                        ESP_LOGI(TAG, "Printer is available and initialized");
                                        
                                        // Extract print data
                                        cJSON *print_data = cJSON_GetObjectItem(root, "print_data");
                                        if (print_data && cJSON_IsObject(print_data))
                                        {
                                            cJSON *title = cJSON_GetObjectItem(print_data, "title");
                                            cJSON *poet_style = cJSON_GetObjectItem(print_data, "poet_style");
                                            cJSON *poem_text = cJSON_GetObjectItem(print_data, "poem_text");
                                            
                                            const char *title_str = (title && cJSON_IsString(title)) ? title->valuestring : "Untitled";
                                            const char *poet_str = (poet_style && cJSON_IsString(poet_style)) ? poet_style->valuestring : "General";
                                            const char *poem_str = (poem_text && cJSON_IsString(poem_text)) ? poem_text->valuestring : "";
                                            
                                            ESP_LOGI(TAG, "=== RECEIVED POEM ===");
                                            ESP_LOGI(TAG, "Title: %s", title_str);
                                            ESP_LOGI(TAG, "Style: %s", poet_str);
                                            ESP_LOGI(TAG, "Poem Text:");
                                            ESP_LOGI(TAG, "%s", poem_str);
                                            ESP_LOGI(TAG, "====================");
                                            
                                            thermal_printer_print_poem(self->printer, title_str, poet_str, poem_str);
                                        }
                                        else
                                        {
                                            ESP_LOGW(TAG, "Print command received but no print_data found");
                                            
                                            // Flash red to indicate error (no print data)
                                            if (self->led_ring)
                                            {
                                                int led_count = self->led_ring->num_leds;
                                                
                                                // Quick red flash 3 times
                                                for (int flash = 0; flash < 3; flash++)
                                                {
                                                    // Flash on
                                                    for (int i = 0; i < led_count; i++)
                                                    {
                                                        self->led_ring->set_pixel(self->led_ring, i, 255, 0, 0);
                                                    }
                                                    self->led_ring->refresh(self->led_ring);
                                                    vTaskDelay(pdMS_TO_TICKS(200));
                                                    
                                                    // Flash off
                                                    self->led_ring->clear(self->led_ring);
                                                    self->led_ring->refresh(self->led_ring);
                                                    vTaskDelay(pdMS_TO_TICKS(200));
                                                }
                                            }
                                        }
                                    }
                                    else
                                    {
                                        ESP_LOGW(TAG, "Print command received but printer not available (printer=%p, initialized=%d)", 
                                                 self->printer, self->printer ? self->printer->initialized : 0);
                                        
                                        // Flash red to indicate printer error
                                        if (self->led_ring)
                                        {
                                            int led_count = self->led_ring->num_leds;
                                            
                                            // Quick red flash 3 times
                                            for (int flash = 0; flash < 3; flash++)
                                            {
                                                // Flash on - bright red
                                                for (int i = 0; i < led_count; i++)
                                                {
                                                    self->led_ring->set_pixel(self->led_ring, i, 255, 0, 0);
                                                }
                                                self->led_ring->refresh(self->led_ring);
                                                vTaskDelay(pdMS_TO_TICKS(200));
                                                
                                                // Flash off
                                                self->led_ring->clear(self->led_ring);
                                                self->led_ring->refresh(self->led_ring);
                                                vTaskDelay(pdMS_TO_TICKS(200));
                                            }
                                        }
                                    }
                                }
                                else if (strcmp(command->valuestring, "none") == 0)
                                {
                                    // No command, continue polling (don't log to reduce spam)
                                }
                                else
                                {
                                    ESP_LOGW(TAG, "Unknown command received: '%s'", command->valuestring);
                                }
                            }
                            else
                            {
                                ESP_LOGW(TAG, "No command field in response or not a string");
                            }

                            // Check for settings updates
                            cJSON *settings = cJSON_GetObjectItem(root, "settings");
                            if (settings && cJSON_IsObject(settings))
                            {
                                ESP_LOGI(TAG, "Settings update received from server");
                                bool settings_changed = false;

                                // Update camera settings (modify in memory, don't save yet)
                                cJSON *camera = cJSON_GetObjectItem(settings, "camera");
                                if (camera)
                                {
                                    cJSON *framesize = cJSON_GetObjectItem(camera, "framesize");
                                    if (framesize && cJSON_IsNumber(framesize))
                                    {
                                        self->settings->settings.camera_framesize = framesize->valueint;
                                        settings_changed = true;
                                    }

                                    cJSON *quality = cJSON_GetObjectItem(camera, "quality");
                                    if (quality && cJSON_IsNumber(quality))
                                    {
                                        if (quality->valueint <= 63)
                                        {
                                            self->settings->settings.camera_quality = quality->valueint;
                                            settings_changed = true;
                                        }
                                    }

                                    cJSON *brightness = cJSON_GetObjectItem(camera, "brightness");
                                    if (brightness && cJSON_IsNumber(brightness))
                                    {
                                        if (brightness->valueint >= -2 && brightness->valueint <= 2)
                                        {
                                            self->settings->settings.camera_brightness = brightness->valueint;
                                            settings_changed = true;
                                        }
                                    }

                                    // Handle flip settings
                                    cJSON *vflip = cJSON_GetObjectItem(camera, "vflip");
                                    cJSON *hmirror = cJSON_GetObjectItem(camera, "hmirror");
                                    if ((vflip && cJSON_IsBool(vflip)) || (hmirror && cJSON_IsBool(hmirror)))
                                    {
                                        if (hmirror && cJSON_IsBool(hmirror))
                                        {
                                            self->settings->settings.camera_flip_h = cJSON_IsTrue(hmirror);
                                            settings_changed = true;
                                        }
                                        if (vflip && cJSON_IsBool(vflip))
                                        {
                                            self->settings->settings.camera_flip_v = cJSON_IsTrue(vflip);
                                            settings_changed = true;
                                        }
                                    }

                                    // Handle rotation setting
                                    cJSON *rotation = cJSON_GetObjectItem(camera, "rotation");
                                    if (rotation && cJSON_IsNumber(rotation))
                                    {
                                        int rot = rotation->valueint;
                                        if (rot == 0 || rot == 90 || rot == 180 || rot == 270)
                                        {
                                            self->settings->settings.camera_rotation = rot;
                                            settings_changed = true;
                                            ESP_LOGI(TAG, "Camera rotation updated to %d°", rot);
                                        }
                                    }

                                    // Handle flash enabled setting
                                    cJSON *flash_enabled = cJSON_GetObjectItem(camera, "flash_enabled");
                                    if (flash_enabled && cJSON_IsBool(flash_enabled))
                                    {
                                        self->settings->settings.flash_enabled = cJSON_IsTrue(flash_enabled);
                                        settings_changed = true;
                                        ESP_LOGI(TAG, "Flash enabled updated to %s", self->settings->settings.flash_enabled ? "true" : "false");
                                    }

                                    // Handle self-timer enabled setting
                                    cJSON *self_timer_enabled = cJSON_GetObjectItem(camera, "self_timer_enabled");
                                    if (self_timer_enabled && cJSON_IsBool(self_timer_enabled))
                                    {
                                        self->settings->settings.self_timer_enabled = cJSON_IsTrue(self_timer_enabled);
                                        settings_changed = true;
                                        ESP_LOGI(TAG, "Self-timer enabled updated to %s", self->settings->settings.self_timer_enabled ? "true" : "false");
                                    }

                                    // Handle auto-print enabled setting
                                    cJSON *auto_print_enabled = cJSON_GetObjectItem(camera, "auto_print_enabled");
                                    if (auto_print_enabled && cJSON_IsBool(auto_print_enabled))
                                    {
                                        self->settings->settings.auto_print_enabled = cJSON_IsTrue(auto_print_enabled);
                                        settings_changed = true;
                                        ESP_LOGI(TAG, "Auto-print enabled updated to %s", self->settings->settings.auto_print_enabled ? "true" : "false");
                                    }
                                }

                                // Update poem settings (modify in memory, don't save yet)
                                cJSON *poem = cJSON_GetObjectItem(settings, "poem");
                                if (poem)
                                {
                                    cJSON *poem_style = cJSON_GetObjectItem(poem, "style");
                                    if (poem_style && cJSON_IsString(poem_style))
                                    {
                                        strncpy(self->settings->settings.poem_style, poem_style->valuestring, sizeof(self->settings->settings.poem_style) - 1);
                                        self->settings->settings.poem_style[sizeof(self->settings->settings.poem_style) - 1] = '\0';
                                        settings_changed = true;
                                        ESP_LOGI(TAG, "Poem style updated to: %s", self->settings->settings.poem_style);
                                    }
                                }

                                // Update server settings (modify in memory, don't save yet)
                                cJSON *server = cJSON_GetObjectItem(settings, "server");
                                if (server)
                                {
                                    cJSON *upload_interval = cJSON_GetObjectItem(server, "upload_interval_seconds");
                                    if (upload_interval && cJSON_IsNumber(upload_interval))
                                    {
                                        self->settings->settings.server_upload_interval = upload_interval->valueint;
                                        settings_changed = true;
                                    }

                                    cJSON *poll_interval = cJSON_GetObjectItem(server, "poll_interval_ms");
                                    if (poll_interval && cJSON_IsNumber(poll_interval))
                                    {
                                        if (poll_interval->valueint >= 50 && poll_interval->valueint <= 10000)
                                        {
                                            self->settings->settings.server_poll_interval = poll_interval->valueint;
                                            self->poll_interval_ms = poll_interval->valueint;
                                            settings_changed = true;
                                            ESP_LOGI(TAG, "Poll interval updated to %d ms", self->poll_interval_ms);
                                        }
                                    }
                                }

                                // Update log settings
                                cJSON *log = cJSON_GetObjectItem(settings, "log");
                                if (log)
                                {
                                    cJSON *upload_enabled = cJSON_GetObjectItem(log, "upload_enabled");
                                    if (upload_enabled)
                                    {
                                        self->settings->settings.log_upload_enabled = cJSON_IsTrue(upload_enabled);
                                        settings_changed = true;
                                        ESP_LOGI(TAG, "Log upload %s", self->settings->settings.log_upload_enabled ? "enabled" : "disabled");
                                    }

                                    cJSON *upload_interval = cJSON_GetObjectItem(log, "upload_interval_seconds");
                                    if (upload_interval && cJSON_IsNumber(upload_interval))
                                    {
                                        if (upload_interval->valueint >= 10 && upload_interval->valueint <= 300)
                                        {
                                            self->settings->settings.log_upload_interval = upload_interval->valueint;
                                            settings_changed = true;
                                            ESP_LOGI(TAG, "Log upload interval updated to %d seconds", self->settings->settings.log_upload_interval);
                                        }
                                    }

                                    cJSON *queue_size = cJSON_GetObjectItem(log, "queue_size");
                                    if (queue_size && cJSON_IsNumber(queue_size))
                                    {
                                        if (queue_size->valueint >= 10 && queue_size->valueint <= 1000)
                                        {
                                            self->settings->settings.log_queue_size = queue_size->valueint;
                                            settings_changed = true;
                                            ESP_LOGI(TAG, "Log queue size updated to %d (restart required)", self->settings->settings.log_queue_size);
                                        }
                                    }
                                }

                                // Update LED ring settings
                                cJSON *led_ring = cJSON_GetObjectItem(settings, "led_ring");
                                if (led_ring)
                                {
                                    ESP_LOGI(TAG, "LED ring settings detected in update");
                                    
                                    cJSON *brightness = cJSON_GetObjectItem(led_ring, "brightness");
                                    if (brightness && cJSON_IsNumber(brightness))
                                    {
                                        uint8_t new_brightness = brightness->valueint;
                                        if (new_brightness <= 100)
                                        {
                                            self->settings->settings.led_ring_brightness = new_brightness;
                                            settings_changed = true;
                                            
                                            // Update LED ring brightness immediately
                                            if (self->led_ring)
                                            {
                                                self->led_ring->set_brightness(self->led_ring, new_brightness);
                                                // Refresh the menu display to show new brightness
                                                refresh_led_ring_menu();
                                                ESP_LOGI(TAG, "✓ LED ring brightness updated to %d%% and menu refreshed", new_brightness);
                                            }
                                        }
                                    }

                                    cJSON *count = cJSON_GetObjectItem(led_ring, "count");
                                    if (count && cJSON_IsNumber(count))
                                    {
                                        uint8_t new_count = count->valueint;
                                        if (new_count >= 1)
                                        {
                                            self->settings->settings.led_ring_count = new_count;
                                            settings_changed = true;
                                            ESP_LOGI(TAG, "✓ LED ring count updated to %d (restart required)", new_count);
                                        }
                                    }
                                }

                                // Save all settings once if anything changed
                                if (settings_changed)
                                {
                                    self->settings->save_settings(self->settings);
                                }
                            }

                            cJSON_Delete(root);
                        }
                    }
                }
            }

            esp_http_client_close(client);
            esp_http_client_cleanup(client);
        }

        // Wait before next poll
        vTaskDelay(pdMS_TO_TICKS(self->poll_interval_ms));
    }

    ESP_LOGI(TAG, "Polling task stopped");
    polling_task_handle = NULL;
    vTaskDelete(NULL);
}

static esp_err_t remote_control_init_impl(RemoteControl_t *self)
{
    ESP_LOGI(TAG, "Initializing Remote Control");
    ESP_LOGI(TAG, "Server URL: %s", self->server_url);
    ESP_LOGI(TAG, "Poll interval: %d ms", self->poll_interval_ms);
    return ESP_OK;
}

static void remote_control_start_polling_impl(RemoteControl_t *self)
{
    if (self->running)
    {
        ESP_LOGW(TAG, "Polling already running");
        return;
    }

    self->running = true;

    BaseType_t result = xTaskCreate(
        polling_task,
        "remote_poll",
        8192,
        self,
        10, // Higher priority for responsive polling
        &polling_task_handle);

    if (result == pdPASS)
    {
        ESP_LOGI(TAG, "Remote polling task started");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to create polling task");
        self->running = false;
    }
}

static void remote_control_stop_polling_impl(RemoteControl_t *self)
{
    if (!self->running)
    {
        return;
    }

    self->running = false;

    if (polling_task_handle)
    {
        vTaskDelete(polling_task_handle);
        polling_task_handle = NULL;
    }

    ESP_LOGI(TAG, "Remote polling stopped");
}

// Constructor
RemoteControl_t *remote_control_create(SettingsManager_t *settings, HttpClient_t *http_client, LEDRing_t *led_ring, ThermalPrinter_t *printer)
{
    RemoteControl_t *remote_control = malloc(sizeof(RemoteControl_t));
    if (!remote_control)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for RemoteControl");
        return NULL;
    }

    // Get server URL from settings and construct command endpoint base
    strncpy(remote_control->server_url, settings->settings.server_upload_url, sizeof(remote_control->server_url) - 1);

    // Remove /api/capture suffix if present to get base URL
    char *api_capture = strstr(remote_control->server_url, "/api/capture");
    if (api_capture)
    {
        *api_capture = '\0';
    }

    remote_control->poll_interval_ms = settings->settings.server_poll_interval;
    remote_control->settings = settings;
    remote_control->http_client = http_client;
    remote_control->led_ring = led_ring;
    remote_control->printer = printer;
    remote_control->running = false;
    remote_control->init = remote_control_init_impl;
    remote_control->start_polling = remote_control_start_polling_impl;
    remote_control->stop_polling = remote_control_stop_polling_impl;

    return remote_control;
}

// Destructor
void remote_control_destroy(RemoteControl_t *remote_control)
{
    if (remote_control)
    {
        if (remote_control->running)
        {
            remote_control->stop_polling(remote_control);
        }
        free(remote_control);
    }
}
