#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include "esp_err.h"
#include <stddef.h>
#include <stdbool.h>

#define LOG_QUEUE_SIZE 50  // Reduced from 100 to prevent excessive memory use
#define LOG_MESSAGE_MAX_LEN 256

// Log Manager "Class" - Captures and queues ESP_LOG output
typedef struct LogManager_t {
    void *queue_handle;  // Internal queue handle
    char *server_url;
    bool enabled;
    
    // Methods
    esp_err_t (*init)(struct LogManager_t* self, const char* server_url);
    esp_err_t (*send_logs)(struct LogManager_t* self);
    void (*enable)(struct LogManager_t* self);
    void (*disable)(struct LogManager_t* self);
    int (*get_queued_count)(struct LogManager_t* self);
} LogManager_t;

// Constructor
LogManager_t* log_manager_create(void);

// Destructor
void log_manager_destroy(LogManager_t* log_manager);

// Global function to capture logs (called by custom log handler)
void log_manager_capture_log(const char* message);

#endif // LOG_MANAGER_H
