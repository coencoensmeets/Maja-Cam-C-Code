#include "log_manager.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "LOG_MANAGER";

// Global log manager instance for capture function
static LogManager_t *g_log_manager = NULL;

// Internal structure
typedef struct {
    QueueHandle_t log_queue;
    char server_url[128];
    bool enabled;
} log_manager_internal_t;

// Log entry structure
typedef struct {
    char message[LOG_MESSAGE_MAX_LEN];
    int64_t timestamp_ms;
} log_entry_t;

// Custom log output function to capture logs
static int custom_log_handler(const char *fmt, va_list args)
{
    // Format the log message
    char message[LOG_MESSAGE_MAX_LEN];
    int len = vsnprintf(message, sizeof(message), fmt, args);
    
    // Also print to console (default behavior)
    vprintf(fmt, args);
    
    // Capture to queue if log manager is enabled
    if (g_log_manager && g_log_manager->enabled)
    {
        log_manager_capture_log(message);
    }
    
    return len;
}

// Initialize log manager
static esp_err_t log_manager_init_impl(LogManager_t *self, const char* server_url)
{
    log_manager_internal_t *internal = (log_manager_internal_t *)self->queue_handle;
    
    // Create log queue
    internal->log_queue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(log_entry_t));
    if (internal->log_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create log queue");
        return ESP_FAIL;
    }
    
    // Store server URL
    if (server_url)
    {
        snprintf(internal->server_url, sizeof(internal->server_url), "%s", server_url);
    }
    
    internal->enabled = true;
    self->enabled = true;
    
    // Set global instance for capture function
    g_log_manager = self;
    
    // Install custom log handler
    esp_log_set_vprintf(custom_log_handler);
    
    return ESP_OK;
}

// Send queued logs to server
static esp_err_t log_manager_send_logs_impl(LogManager_t *self)
{
    log_manager_internal_t *internal = (log_manager_internal_t *)self->queue_handle;
    
    if (!internal->enabled)
    {
        return ESP_OK;
    }
    
    int log_count = uxQueueMessagesWaiting(internal->log_queue);
    
    if (log_count == 0)
    {
        return ESP_OK; // Nothing to send
    }
    
    // Limit batch size to prevent stack overflow (max 10 logs per request for safety)
    #define MAX_LOGS_PER_BATCH 10
    if (log_count > MAX_LOGS_PER_BATCH)
    {
        log_count = MAX_LOGS_PER_BATCH;
    }
    
    // Build JSON array of logs - use heap allocation to avoid stack overflow
    // Add extra safety margin to buffer size
    size_t buffer_size = log_count * (LOG_MESSAGE_MAX_LEN * 2 + 150) + 500;
    char *json_buffer = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!json_buffer)
    {
        // Try regular heap if PSRAM allocation fails
        json_buffer = malloc(buffer_size);
        if (!json_buffer)
        {
            ESP_LOGE(TAG, "Failed to allocate %d bytes for JSON", buffer_size);
            return ESP_ERR_NO_MEM;
        }
    }
    
    strncpy(json_buffer, "{\"logs\":[" , buffer_size - 1);
    json_buffer[buffer_size - 1] = '\0';
    size_t current_len = strlen(json_buffer);
    
    log_entry_t entry;
    int sent_count = 0;
    bool first = true;
    
    // Allocate escape buffer on heap to avoid stack overflow
    char *escaped_msg = malloc(LOG_MESSAGE_MAX_LEN * 2);
    if (!escaped_msg)
    {
        ESP_LOGE(TAG, "Failed to allocate escape buffer");
        free(json_buffer);
        return ESP_ERR_NO_MEM;
    }
    
    // Dequeue and build JSON (only up to log_count which is limited to MAX_LOGS_PER_BATCH)
    while (sent_count < log_count && xQueueReceive(internal->log_queue, &entry, 0) == pdTRUE)
    {
        // Check if we have enough space left in buffer (safety check)
        if (current_len + (LOG_MESSAGE_MAX_LEN * 2 + 150) >= buffer_size)
        {
            ESP_LOGW(TAG, "JSON buffer nearly full, stopping at %d logs", sent_count);
            break;
        }
        
        if (!first)
        {
            strncat(json_buffer, ",", buffer_size - current_len - 1);
            current_len++;
        }
        first = false;
        
        // Escape special characters in message
        int j = 0;
        size_t msg_len = strnlen(entry.message, LOG_MESSAGE_MAX_LEN);
        for (int i = 0; i < msg_len && j < (LOG_MESSAGE_MAX_LEN * 2 - 2); i++)
        {
            char c = entry.message[i];
            if (c == '"' || c == '\\')
            {
                escaped_msg[j++] = '\\';
            }
            if (c == '\n')
            {
                escaped_msg[j++] = '\\';
                escaped_msg[j++] = 'n';
                continue;
            }
            if (c == '\r')
            {
                continue; // Skip carriage returns
            }
            escaped_msg[j++] = c;
        }
        escaped_msg[j] = '\0';
        
        // Add log entry to JSON with bounds checking
        char log_json[LOG_MESSAGE_MAX_LEN * 2 + 100];
        int written = snprintf(log_json, sizeof(log_json),
                 "{\"timestamp\":%lld,\"message\":\"%s\"}",
                 entry.timestamp_ms, escaped_msg);
        
        // Verify snprintf didn't truncate
        if (written < 0 || written >= sizeof(log_json))
        {
            ESP_LOGW(TAG, "Log entry too large, skipping");
            continue;
        }
        
        // Check if we have space to add this entry
        if (current_len + written + 10 >= buffer_size)
        {
            ESP_LOGW(TAG, "Buffer full, stopping at %d logs", sent_count);
            break;
        }
        
        strncat(json_buffer, log_json, buffer_size - current_len - 1);
        current_len += written;
        
        sent_count++;
    }
    
    // Safely close JSON array
    if (current_len + 3 < buffer_size)
    {
        strncat(json_buffer, "]}", buffer_size - current_len - 1);
    }
    else
    {
        ESP_LOGE(TAG, "Buffer overflow prevented, truncating JSON");
        // Force proper JSON closure even if truncated
        json_buffer[buffer_size - 3] = ']';
        json_buffer[buffer_size - 2] = '}';
        json_buffer[buffer_size - 1] = '\0';
    }
    
    // Send to server with retry logic
    esp_http_client_config_t config = {
        .url = internal->server_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000,  // 30 second timeout
        .buffer_size = 4096,  // Limit buffer size
        .buffer_size_tx = 4096,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for logs");
        free(escaped_msg);
        free(json_buffer);
        return ESP_FAIL;
    }
    
    // Set headers with error checking
    esp_err_t header_err = esp_http_client_set_header(client, "Content-Type", "application/json");
    if (header_err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set Content-Type header");
        esp_http_client_cleanup(client);
        free(escaped_msg);
        free(json_buffer);
        return header_err;
    }
    
    // Verify buffer is valid before sending
    size_t json_len = strlen(json_buffer);
    if (json_len == 0 || json_len >= buffer_size)
    {
        ESP_LOGE(TAG, "Invalid JSON buffer (len=%d, max=%d)", json_len, buffer_size);
        esp_http_client_cleanup(client);
        free(escaped_msg);
        free(json_buffer);
        return ESP_FAIL;
    }
    esp_http_client_set_post_field(client, json_buffer, json_len);
    
    // Retry logic: Try up to 3 times with exponential backoff
    esp_err_t err = ESP_FAIL;
    const int MAX_RETRIES = 3;
    
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++)
    {
        if (attempt > 1)
        {
            ESP_LOGW(TAG, "Retrying log upload (attempt %d/%d)...", attempt, MAX_RETRIES);
            // Exponential backoff: 1s, 2s, 4s
            vTaskDelay(pdMS_TO_TICKS((1 << (attempt - 1)) * 1000));
        }
        
        err = esp_http_client_perform(client);
        
        if (err == ESP_OK)
        {
            int status_code = esp_http_client_get_status_code(client);
            if (status_code == 200)
            {
                // Logs sent successfully
                break; // Success!
            }
            else
            {
                ESP_LOGW(TAG, "Server returned status %d for logs", status_code);
                err = ESP_FAIL;
                break; // Don't retry on server errors
            }
        }
        else
        {
            ESP_LOGW(TAG, "Failed to send logs (attempt %d/%d): %s", 
                     attempt, MAX_RETRIES, esp_err_to_name(err));
            
            if (attempt == MAX_RETRIES)
            {
                ESP_LOGE(TAG, "All %d attempts to send logs failed", MAX_RETRIES);
            }
            else
            {
                ESP_LOGW(TAG, "Attempt %d failed, retrying...", attempt);
                vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1s before retry
            }
        }
    }
    
    // Cleanup in reverse order
    esp_http_client_cleanup(client);
    free(escaped_msg);
    free(json_buffer);
    
    return err;
}

// Enable log capture
static void log_manager_enable_impl(LogManager_t *self)
{
    log_manager_internal_t *internal = (log_manager_internal_t *)self->queue_handle;
    internal->enabled = true;
    self->enabled = true;
}

// Disable log capture
static void log_manager_disable_impl(LogManager_t *self)
{
    log_manager_internal_t *internal = (log_manager_internal_t *)self->queue_handle;
    internal->enabled = false;
    self->enabled = false;
}

// Get queued log count
static int log_manager_get_queued_count_impl(LogManager_t *self)
{
    log_manager_internal_t *internal = (log_manager_internal_t *)self->queue_handle;
    return uxQueueMessagesWaiting(internal->log_queue);
}

// Constructor
LogManager_t *log_manager_create(void)
{
    LogManager_t *log_manager = malloc(sizeof(LogManager_t));
    if (!log_manager)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for Log Manager");
        return NULL;
    }
    
    log_manager_internal_t *internal = malloc(sizeof(log_manager_internal_t));
    if (!internal)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for Log Manager internal");
        free(log_manager);
        return NULL;
    }
    
    memset(internal, 0, sizeof(log_manager_internal_t));
    
    log_manager->queue_handle = internal;
    log_manager->server_url = internal->server_url;
    log_manager->enabled = false;
    
    // Assign methods
    log_manager->init = log_manager_init_impl;
    log_manager->send_logs = log_manager_send_logs_impl;
    log_manager->enable = log_manager_enable_impl;
    log_manager->disable = log_manager_disable_impl;
    log_manager->get_queued_count = log_manager_get_queued_count_impl;
    
    return log_manager;
}

// Destructor
void log_manager_destroy(LogManager_t *log_manager)
{
    if (log_manager)
    {
        if (log_manager == g_log_manager)
        {
            g_log_manager = NULL;
        }
        
        if (log_manager->queue_handle)
        {
            log_manager_internal_t *internal = (log_manager_internal_t *)log_manager->queue_handle;
            
            if (internal->log_queue)
            {
                vQueueDelete(internal->log_queue);
            }
            
            free(internal);
        }
        
        free(log_manager);
    }
}

// Global function to capture logs
void log_manager_capture_log(const char* message)
{
    if (!g_log_manager || !g_log_manager->enabled)
    {
        return;
    }
    
    log_manager_internal_t *internal = (log_manager_internal_t *)g_log_manager->queue_handle;
    
    if (!internal || !internal->log_queue)
    {
        return;
    }
    
    // Get current timestamp
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t timestamp_ms = (int64_t)tv.tv_sec * 1000LL + (int64_t)tv.tv_usec / 1000LL;
    
    // Create log entry
    log_entry_t entry;
    snprintf(entry.message, sizeof(entry.message), "%s", message);
    entry.timestamp_ms = timestamp_ms;
    
    // Try to add to queue (don't block)
    if (xQueueSend(internal->log_queue, &entry, 0) != pdTRUE)
    {
        // Queue is full - drop oldest log and add new one (FIFO circular buffer behavior)
        // This prevents queue overflow and ensures newest logs are always captured
        log_entry_t old_entry;
        xQueueReceive(internal->log_queue, &old_entry, 0);
        xQueueSend(internal->log_queue, &entry, 0);
    }
}
