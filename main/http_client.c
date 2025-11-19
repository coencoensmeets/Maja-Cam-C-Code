#include "http_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "HTTP_CLIENT";

// Boundary for multipart form data
static const char *BOUNDARY = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

// Global reference for the task (to support stop functionality)
static TaskHandle_t g_upload_task_handle = NULL;
static HttpClient_t *g_http_client = NULL;

// ============================================================================
// HTTP Upload Implementation
// ============================================================================

static esp_err_t upload_image_impl(HttpClient_t *self, camera_fb_t *fb)
{
    if (!fb)
    {
        ESP_LOGE(TAG, "No image data to upload");
        return ESP_FAIL;
    }

    if (!self->settings->settings.server_upload_enabled)
    {
        ESP_LOGW(TAG, "Upload is disabled in settings");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Uploading image to server... Size: %zu bytes", fb->len);

    // Prepare HTTP headers
    char content_type[128];
    snprintf(content_type, sizeof(content_type),
             "multipart/form-data; boundary=%s", BOUNDARY);

    // Build multipart form data with settings
    const char *auto_print_field =
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"auto_print_enabled\"\r\n\r\n"
        "%s\r\n";
    
    const char *poem_style_field =
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"poem_style\"\r\n\r\n"
        "%s\r\n";
    
    const char *image_header_fmt =
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"image\"; filename=\"esp32_capture.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n";

    const char *footer_fmt = "\r\n--%s--\r\n";

    // Build the form fields
    char auto_print_part[256];
    char poem_style_part[256];
    char image_header[256];
    char footer[100];
    
    snprintf(auto_print_part, sizeof(auto_print_part), auto_print_field, 
             BOUNDARY, self->settings->settings.auto_print_enabled ? "true" : "false");
    snprintf(poem_style_part, sizeof(poem_style_part), poem_style_field,
             BOUNDARY, self->settings->settings.poem_style);
    snprintf(image_header, sizeof(image_header), image_header_fmt, BOUNDARY);
    snprintf(footer, sizeof(footer), footer_fmt, BOUNDARY);

    size_t auto_print_len = strlen(auto_print_part);
    size_t poem_style_len = strlen(poem_style_part);
    size_t image_header_len = strlen(image_header);
    size_t footer_len = strlen(footer);
    size_t total_len = auto_print_len + poem_style_len + image_header_len + fb->len + footer_len;

    // Allocate buffer for complete POST data
    uint8_t *post_data = malloc(total_len);
    if (!post_data)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for POST data");
        return ESP_FAIL;
    }

    // Build the POST data with all parts
    size_t offset = 0;
    memcpy(post_data + offset, auto_print_part, auto_print_len);
    offset += auto_print_len;
    memcpy(post_data + offset, poem_style_part, poem_style_len);
    offset += poem_style_len;
    memcpy(post_data + offset, image_header, image_header_len);
    offset += image_header_len;
    memcpy(post_data + offset, fb->buf, fb->len);
    offset += fb->len;
    memcpy(post_data + offset, footer, footer_len);

    // Configure HTTP client with longer timeout and retry capability
    esp_http_client_config_t config = {
        .url = self->server_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000, // 30 second timeout (increased from 10s)
        .disable_auto_redirect = false,
        .max_redirection_count = 5,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(post_data);
        return ESP_FAIL;
    }

    // Set headers
    esp_http_client_set_header(client, "Content-Type", content_type);

    // Set POST data
    esp_http_client_set_post_field(client, (char *)post_data, total_len);

    // Retry logic: Try up to 3 times with exponential backoff
    esp_err_t err = ESP_FAIL;
    const int MAX_RETRIES = 3;
    int retry_delay_ms = 1000; // Start with 1 second
    
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++)
    {
        if (attempt > 1)
        {
            ESP_LOGW(TAG, "Retry attempt %d/%d (waiting %dms)...", attempt, MAX_RETRIES, retry_delay_ms);
            vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
            retry_delay_ms *= 2; // Exponential backoff: 1s, 2s, 4s
        }
        else
        {
            ESP_LOGI(TAG, "Upload attempt %d/%d to %s", attempt, MAX_RETRIES, self->server_url);
        }

        // Perform HTTP request
        err = esp_http_client_perform(client);

        if (err == ESP_OK)
        {
            int status_code = esp_http_client_get_status_code(client);
            int content_length = esp_http_client_get_content_length(client);

            ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                     status_code, content_length);

            if (status_code == 200)
            {
                ESP_LOGI(TAG, "✓ Image uploaded successfully!");
                break; // Success! Exit retry loop
            }
            else
            {
                ESP_LOGE(TAG, "✗ Server returned error status: %d", status_code);
                err = ESP_FAIL;
                // Don't retry on server errors (4xx, 5xx) - only on connection errors
                break;
            }
        }
        else
        {
            // Detailed error logging
            ESP_LOGE(TAG, "✗ HTTP request failed (attempt %d/%d): %s (0x%x)", 
                     attempt, MAX_RETRIES, esp_err_to_name(err), err);
            
            // Log specific error types
            if (err == ESP_ERR_HTTP_CONNECT)
            {
                ESP_LOGE(TAG, "   → Connection failed. Check if server is reachable at %s", self->server_url);
                ESP_LOGE(TAG, "   → Verify: 1) Server is running, 2) IP is correct, 3) No firewall blocking");
            }
            else if (err == ESP_ERR_HTTP_WRITE_DATA)
            {
                ESP_LOGE(TAG, "   → Failed to send data to server");
            }
            else if (err == ESP_ERR_HTTP_FETCH_HEADER)
            {
                ESP_LOGE(TAG, "   → Failed to receive response from server");
            }
            
            // Continue to next retry attempt if we haven't exhausted retries
            if (attempt == MAX_RETRIES)
            {
                ESP_LOGE(TAG, "✗ All %d upload attempts failed", MAX_RETRIES);
            }
        }
    }

    // Cleanup
    esp_http_client_cleanup(client);
    free(post_data);

    return err;
}

static esp_err_t capture_and_upload_impl(HttpClient_t *self)
{
    if (!self->camera || !self->camera->initialized)
    {
        ESP_LOGE(TAG, "Camera not initialized");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Capturing image...");
    camera_fb_t *fb = self->camera->capture(self->camera);

    if (!fb)
    {
        ESP_LOGE(TAG, "Failed to capture image");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Image captured: %zu bytes", fb->len);

    // Upload to server
    esp_err_t err = self->upload_image(self, fb);

    // Return frame buffer
    self->camera->return_frame(self->camera, fb);

    return err;
}

// ============================================================================
// Auto Upload Task
// ============================================================================

static void auto_upload_task(void *pvParameters)
{
    HttpClient_t *client = (HttpClient_t *)pvParameters;

    ESP_LOGI(TAG, "Auto-upload task started");
    ESP_LOGI(TAG, "Upload interval: %lu seconds", client->settings->settings.server_upload_interval);
    ESP_LOGI(TAG, "Upload URL: %s", client->server_url);

    while (client->running)
    {
        // Check if upload is still enabled
        if (!client->settings->settings.server_upload_enabled)
        {
            ESP_LOGI(TAG, "Upload disabled, pausing...");
            vTaskDelay(pdMS_TO_TICKS(5000)); // Check again in 5 seconds
            continue;
        }

        ESP_LOGI(TAG, "Starting scheduled capture and upload...");

        esp_err_t ret = client->capture_and_upload(client);

        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "✓ Scheduled upload completed successfully");
        }
        else
        {
            ESP_LOGE(TAG, "✗ Scheduled upload failed");
        }

        // Wait for the configured interval
        uint32_t delay_ms = client->settings->settings.server_upload_interval * 1000;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    ESP_LOGI(TAG, "Auto-upload task stopped");
    g_upload_task_handle = NULL;
    vTaskDelete(NULL);
}

static void start_auto_upload_task_impl(HttpClient_t *self)
{
    if (g_upload_task_handle != NULL)
    {
        ESP_LOGW(TAG, "Auto-upload task already running");
        return;
    }

    if (!self->settings->settings.server_upload_enabled)
    {
        ESP_LOGW(TAG, "Auto-upload is disabled in settings");
        return;
    }

    self->running = true;
    g_http_client = self;

    xTaskCreate(
        auto_upload_task,
        "auto_upload_task",
        8192,                 // Stack size
        (void *)self,         // Parameters
        5,                    // Priority
        &g_upload_task_handle // Task handle
    );

    ESP_LOGI(TAG, "Auto-upload task started (interval: %lu seconds)",
             self->settings->settings.server_upload_interval);
}

static void stop_auto_upload_task_impl(HttpClient_t *self)
{
    if (g_upload_task_handle == NULL)
    {
        ESP_LOGW(TAG, "Auto-upload task not running");
        return;
    }

    self->running = false;
    ESP_LOGI(TAG, "Stopping auto-upload task...");

    // Wait for task to finish (up to 5 seconds)
    int timeout = 50; // 50 * 100ms = 5 seconds
    while (g_upload_task_handle != NULL && timeout > 0)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        timeout--;
    }

    if (g_upload_task_handle != NULL)
    {
        ESP_LOGW(TAG, "Task did not stop gracefully, deleting...");
        vTaskDelete(g_upload_task_handle);
        g_upload_task_handle = NULL;
    }

    ESP_LOGI(TAG, "Auto-upload task stopped");
}

// ============================================================================
// Initialization
// ============================================================================

static esp_err_t init_impl(HttpClient_t *self)
{
    // Copy URL from settings
    strncpy(self->server_url, self->settings->settings.server_upload_url,
            sizeof(self->server_url) - 1);

    ESP_LOGI(TAG, "HTTP Client initialized");
    ESP_LOGI(TAG, "Server URL: %s", self->server_url);
    ESP_LOGI(TAG, "Upload enabled: %s",
             self->settings->settings.server_upload_enabled ? "Yes" : "No");

    return ESP_OK;
}

// ============================================================================
// Constructor/Destructor
// ============================================================================

HttpClient_t *http_client_create(Camera_t *camera, SettingsManager_t *settings)
{
    if (!camera || !settings)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return NULL;
    }

    HttpClient_t *client = malloc(sizeof(HttpClient_t));
    if (!client)
    {
        ESP_LOGE(TAG, "Failed to allocate memory");
        return NULL;
    }

    memset(client, 0, sizeof(HttpClient_t));

    client->camera = camera;
    client->settings = settings;
    client->running = false;

    // Assign methods
    client->init = init_impl;
    client->upload_image = upload_image_impl;
    client->capture_and_upload = capture_and_upload_impl;
    client->start_auto_upload_task = start_auto_upload_task_impl;
    client->stop_auto_upload_task = stop_auto_upload_task_impl;

    return client;
}

void http_client_destroy(HttpClient_t *client)
{
    if (client)
    {
        if (client->running)
        {
            client->stop_auto_upload_task(client);
        }
        free(client);
    }
}
