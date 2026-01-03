#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include <stdbool.h>
#include <stdint.h>

// Update channel types
typedef enum {
    OTA_CHANNEL_RELEASE,    // Official GitHub releases
    OTA_CHANNEL_TESTING     // Testing branch
} ota_channel_t;

// OTA update status
typedef enum {
    OTA_STATUS_IDLE,
    OTA_STATUS_CHECKING,
    OTA_STATUS_DOWNLOADING,
    OTA_STATUS_INSTALLING,
    OTA_STATUS_SUCCESS,
    OTA_STATUS_ERROR
} ota_status_t;

// GitHub release information
typedef struct {
    char version[64];           // Version tag (e.g., "v1.0.0" or "branch-testing")
    char download_url[512];     // URL to firmware binary
    char description[512];      // Release notes
    bool is_prerelease;         // Is this a pre-release?
    uint32_t file_size;         // Size of firmware in bytes
} github_release_t;

// OTA progress callback
typedef void (*ota_progress_callback_t)(int progress_percent, const char *status_msg);

// OTA Manager "Class"
typedef struct OTAManager_t
{
    char github_owner[64];      // GitHub repository owner
    char github_repo[64];       // GitHub repository name
    char testing_branch[32];    // Branch name for testing builds
    ota_channel_t channel;      // Current update channel
    bool auto_check;            // Auto-check for updates on startup
    
    ota_status_t status;
    char current_version[32];   // Current firmware version
    github_release_t latest_release;
    ota_progress_callback_t progress_callback;
    
    int64_t last_check_time;    // Timestamp of last GitHub API check (microseconds)
    bool initialized;

    // Methods
    esp_err_t (*init)(struct OTAManager_t *self, const char *owner, const char *repo, const char *branch);
    esp_err_t (*check_for_update)(struct OTAManager_t *self, bool *update_available);
    esp_err_t (*perform_update)(struct OTAManager_t *self);
    esp_err_t (*set_channel)(struct OTAManager_t *self, ota_channel_t channel);
    esp_err_t (*set_auto_check)(struct OTAManager_t *self, bool enabled);
    esp_err_t (*get_current_version)(struct OTAManager_t *self, char *version_out);
    esp_err_t (*get_latest_version)(struct OTAManager_t *self, char *version_out);
    void (*set_progress_callback)(struct OTAManager_t *self, ota_progress_callback_t callback);
    const char* (*get_status_string)(struct OTAManager_t *self);
    
    // GitHub API interaction
    esp_err_t (*fetch_latest_release)(struct OTAManager_t *self);
    esp_err_t (*fetch_testing_build)(struct OTAManager_t *self);
    esp_err_t (*download_and_install)(struct OTAManager_t *self, const char *url);
    
    // Utility
    void (*print_info)(struct OTAManager_t *self);
} OTAManager_t;

// Constructor
OTAManager_t *ota_manager_create(void);

// Destructor
void ota_manager_destroy(OTAManager_t *manager);

// GitHub API endpoints
#define GITHUB_API_RELEASES "https://api.github.com/repos/%s/%s/releases/latest"
#define GITHUB_API_BRANCH_COMMITS "https://api.github.com/repos/%s/%s/commits/%s"
#define GITHUB_RAW_CONTENT "https://raw.githubusercontent.com/%s/%s/%s/%s"

// Firmware version - should match your build version
#define FIRMWARE_VERSION "v1.0.0-test.12"

// OTA buffer size
#define OTA_BUFFER_SIZE 1024

// Timeouts
#define OTA_HTTP_TIMEOUT_MS 15000       // HTTP request timeout
#define OTA_DOWNLOAD_TIMEOUT_MS 60000   // Firmware download timeout
#define OTA_MIN_CHECK_INTERVAL_MS 60000 // Minimum 60 seconds between GitHub API checks

// Limits
#define OTA_MAX_JSON_SIZE 102400        // Max 100KB for GitHub API JSON response
#define OTA_MIN_FIRMWARE_SIZE 102400    // Min 100KB for firmware (sanity check)
#define OTA_MAX_FIRMWARE_SIZE 8388608   // Max 8MB for firmware

#endif // OTA_MANAGER_H
