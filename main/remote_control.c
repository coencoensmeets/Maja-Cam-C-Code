#include "remote_control.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
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
                                if (strcmp(command->valuestring, "capture") == 0)
                                {
                                    ESP_LOGI(TAG, "Capture command received from server");
                                    self->http_client->capture_and_upload(self->http_client);
                                }
                                else if (strcmp(command->valuestring, "none") == 0)
                                {
                                    // No command, continue polling
                                }
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
RemoteControl_t *remote_control_create(SettingsManager_t *settings, HttpClient_t *http_client)
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
