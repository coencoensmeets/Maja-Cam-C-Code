#include "ota_manager.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "OTA";

// ============================================================================
// HTTP Event Handler for Progress Tracking
// ============================================================================

typedef struct {
    int total_size;
    int downloaded;
    ota_progress_callback_t callback;
} ota_download_context_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_HEADER:
            if (strcasecmp(evt->header_key, "Content-Length") == 0) {
                ota_download_context_t *ctx = (ota_download_context_t *)evt->user_data;
                if (ctx) {
                    ctx->total_size = atoi(evt->header_value);
                    ESP_LOGI(TAG, "Firmware size: %d bytes", ctx->total_size);
                }
            }
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                ota_download_context_t *ctx = (ota_download_context_t *)evt->user_data;
                if (ctx) {
                    ctx->downloaded += evt->data_len;
                    int progress = (ctx->downloaded * 100) / ctx->total_size;
                    if (ctx->callback) {
                        char msg[64];
                        snprintf(msg, sizeof(msg), "Downloaded %d%%", progress);
                        ctx->callback(progress, msg);
                    }
                }
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

// ============================================================================
// GitHub API Functions
// ============================================================================

static esp_err_t fetch_latest_release_impl(OTAManager_t *self)
{
    if (!self) {
        ESP_LOGE(TAG, "OTA manager is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!self->initialized) {
        ESP_LOGE(TAG, "OTA manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Clear previous release info
    memset(&self->latest_release, 0, sizeof(self->latest_release));

    self->status = OTA_STATUS_CHECKING;
    ESP_LOGI(TAG, "Checking for latest release from %s/%s", self->github_owner, self->github_repo);

    char url[512];
    int written = snprintf(url, sizeof(url), GITHUB_API_RELEASES, self->github_owner, self->github_repo);
    if (written < 0 || written >= sizeof(url)) {
        ESP_LOGE(TAG, "URL buffer overflow");
        self->status = OTA_STATUS_ERROR;
        return ESP_ERR_INVALID_SIZE;
    }

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 15000,  // Increased to 15 seconds for API calls
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 4096,  // Larger buffer for better performance
        .buffer_size_tx = 1024,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        self->status = OTA_STATUS_ERROR;
        return ESP_FAIL;
    }
    
    esp_err_t err = esp_http_client_perform(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to fetch release info: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        self->status = OTA_STATUS_ERROR;
        return err;
    }

    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP error: %d", status_code);
        esp_http_client_cleanup(client);
        self->status = OTA_STATUS_ERROR;
        return ESP_FAIL;
    }

    int content_length = esp_http_client_get_content_length(client);
    
    // Validate content length
    if (content_length <= 0 || content_length > 102400) {  // Max 100KB for JSON
        ESP_LOGE(TAG, "Invalid content length: %d", content_length);
        esp_http_client_cleanup(client);
        self->status = OTA_STATUS_ERROR;
        return ESP_FAIL;
    }
    
    char *buffer = malloc(content_length + 1);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for response", content_length + 1);
        esp_http_client_cleanup(client);
        self->status = OTA_STATUS_ERROR;
        return ESP_ERR_NO_MEM;
    }

    int read_len = esp_http_client_read(client, buffer, content_length);
    if (read_len <= 0) {
        ESP_LOGE(TAG, "Failed to read response data");
        free(buffer);
        esp_http_client_cleanup(client);
        self->status = OTA_STATUS_ERROR;
        return ESP_FAIL;
    }
    buffer[read_len] = '\0';
    esp_http_client_cleanup(client);

    // Parse JSON response
    cJSON *root = cJSON_Parse(buffer);
    free(buffer);

    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        self->status = OTA_STATUS_ERROR;
        return ESP_FAIL;
    }

    // Extract release information
    cJSON *tag = cJSON_GetObjectItem(root, "tag_name");
    cJSON *prerelease = cJSON_GetObjectItem(root, "prerelease");
    cJSON *body = cJSON_GetObjectItem(root, "body");
    cJSON *assets = cJSON_GetObjectItem(root, "assets");

    if (tag && cJSON_IsString(tag) && tag->valuestring) {
        strncpy(self->latest_release.version, tag->valuestring, sizeof(self->latest_release.version) - 1);
        self->latest_release.version[sizeof(self->latest_release.version) - 1] = '\0';
    } else {
        ESP_LOGE(TAG, "Missing or invalid tag_name in release");
        cJSON_Delete(root);
        self->status = OTA_STATUS_ERROR;
        return ESP_FAIL;
    }

    if (prerelease && cJSON_IsBool(prerelease)) {
        self->latest_release.is_prerelease = cJSON_IsTrue(prerelease);
    }

    if (body && cJSON_IsString(body) && body->valuestring) {
        strncpy(self->latest_release.description, body->valuestring, sizeof(self->latest_release.description) - 1);
        self->latest_release.description[sizeof(self->latest_release.description) - 1] = '\0';
    }

    // Find the .bin firmware file in assets
    if (assets && cJSON_IsArray(assets)) {
        int asset_count = cJSON_GetArraySize(assets);
        if (asset_count == 0) {
            ESP_LOGW(TAG, "No assets found in release");
        }
        
        cJSON *asset = NULL;
        cJSON_ArrayForEach(asset, assets) {
            cJSON *name = cJSON_GetObjectItem(asset, "name");
            if (name && cJSON_IsString(name) && name->valuestring && strstr(name->valuestring, ".bin")) {
                cJSON *download_url = cJSON_GetObjectItem(asset, "browser_download_url");
                cJSON *size = cJSON_GetObjectItem(asset, "size");
                
                if (download_url && cJSON_IsString(download_url) && download_url->valuestring) {
                    size_t url_len = strlen(download_url->valuestring);
                    if (url_len > 0 && url_len < sizeof(self->latest_release.download_url)) {
                        strncpy(self->latest_release.download_url, download_url->valuestring, 
                               sizeof(self->latest_release.download_url) - 1);
                        self->latest_release.download_url[sizeof(self->latest_release.download_url) - 1] = '\0';
                    } else {
                        ESP_LOGE(TAG, "Download URL too long: %d bytes", url_len);
                        continue;
                    }
                }
                
                if (size && cJSON_IsNumber(size)) {
                    self->latest_release.file_size = size->valueint;
                    ESP_LOGI(TAG, "Firmware size: %d bytes", self->latest_release.file_size);
                }
                break;
            }
        }
    } else {
        ESP_LOGW(TAG, "No assets array in release");
    }

    cJSON_Delete(root);

    if (strlen(self->latest_release.download_url) == 0) {
        ESP_LOGE(TAG, "No firmware binary found in release");
        self->status = OTA_STATUS_ERROR;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Latest release: %s (prerelease: %s)", 
             self->latest_release.version, 
             self->latest_release.is_prerelease ? "yes" : "no");
    ESP_LOGI(TAG, "Download URL: %s", self->latest_release.download_url);

    self->status = OTA_STATUS_IDLE;
    return ESP_OK;
}

static esp_err_t fetch_testing_build_impl(OTAManager_t *self)
{
    if (!self->initialized) {
        ESP_LOGE(TAG, "OTA manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    self->status = OTA_STATUS_CHECKING;
    ESP_LOGI(TAG, "Checking for testing build from branch: %s", self->testing_branch);

    // For testing builds, we construct a direct download URL to the artifact
    // This assumes you have a GitHub Actions workflow that builds the firmware
    // and uploads it as an artifact or to GitHub Releases with a "testing" tag
    
    // Option 1: Use a specific testing release tag
    snprintf(self->latest_release.version, sizeof(self->latest_release.version), 
             "%s-testing", self->testing_branch);
    
    // Construct URL to raw firmware binary from testing branch
    // Format: https://github.com/owner/repo/releases/download/testing/firmware.bin
    snprintf(self->latest_release.download_url, sizeof(self->latest_release.download_url),
             "https://github.com/%s/%s/releases/download/testing/firmware.bin",
             self->github_owner, self->github_repo);
    
    self->latest_release.is_prerelease = true;
    strncpy(self->latest_release.description, "Testing build from development branch", 
           sizeof(self->latest_release.description) - 1);

    ESP_LOGI(TAG, "Testing build URL: %s", self->latest_release.download_url);
    
    self->status = OTA_STATUS_IDLE;
    return ESP_OK;
}

// ============================================================================
// OTA Update Functions
// ============================================================================

static esp_err_t download_and_install_impl(OTAManager_t *self, const char *url)
{
    if (!self) {
        ESP_LOGE(TAG, "OTA manager is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!self->initialized) {
        ESP_LOGE(TAG, "OTA manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!url || strlen(url) == 0) {
        ESP_LOGE(TAG, "Invalid download URL");
        self->status = OTA_STATUS_ERROR;
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate URL format (must be HTTPS)
    if (strncmp(url, "https://", 8) != 0) {
        ESP_LOGE(TAG, "URL must use HTTPS protocol: %s", url);
        self->status = OTA_STATUS_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    self->status = OTA_STATUS_DOWNLOADING;
    ESP_LOGI(TAG, "Starting OTA update from: %s", url);

    ota_download_context_t download_ctx = {
        .total_size = 0,
        .downloaded = 0,
        .callback = self->progress_callback
    };

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 60000,  // 60 seconds for firmware download
        .keep_alive_enable = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_event_handler,
        .user_data = &download_ctx,
        .buffer_size = 4096,  // Larger buffer for better download speed
        .buffer_size_tx = 1024,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
        .bulk_flash_erase = true,  // Erase entire partition first for reliability
        .partial_http_download = false,  // Download complete image
    };

    // Verify we have a valid update partition before starting
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No OTA update partition available");
        self->status = OTA_STATUS_ERROR;
        if (self->progress_callback) {
            self->progress_callback(0, "No update partition!");
        }
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Update partition: %s at 0x%08x (size: %d KB)", 
             update_partition->label, update_partition->address, update_partition->size / 1024);

    self->status = OTA_STATUS_INSTALLING;
    ESP_LOGI(TAG, "Starting OTA installation...");

    esp_err_t err = esp_https_ota(&ota_config);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "✓ OTA update successful! Restarting...");
        self->status = OTA_STATUS_SUCCESS;
        if (self->progress_callback) {
            self->progress_callback(100, "Update complete! Restarting...");
        }
        
        // Mark the new partition as valid
        const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
        if (update_partition) {
            esp_ota_set_boot_partition(update_partition);
            ESP_LOGI(TAG, "Next boot partition set to: %s", update_partition->label);
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "✗ OTA update failed: %s", esp_err_to_name(err));
        self->status = OTA_STATUS_ERROR;
        if (self->progress_callback) {
            char error_msg[64];
            snprintf(error_msg, sizeof(error_msg), "Update failed: %s", esp_err_to_name(err));
            self->progress_callback(0, error_msg);
        }
        
        // Log additional diagnostic info
        const esp_partition_t *running = esp_ota_get_running_partition();
        ESP_LOGE(TAG, "Remaining on partition: %s", running ? running->label : "unknown");
    }

    return err;
}

static esp_err_t perform_update_impl(OTAManager_t *self)
{
    if (!self->initialized) {
        ESP_LOGE(TAG, "OTA manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (strlen(self->latest_release.download_url) == 0) {
        ESP_LOGE(TAG, "No download URL available. Check for updates first.");
        return ESP_FAIL;
    }

    return self->download_and_install(self, self->latest_release.download_url);
}

// ============================================================================
// Version Comparison
// ============================================================================

static int compare_versions(const char *v1, const char *v2)
{
    // Simple version comparison - expects format "vX.Y.Z"
    // Returns: -1 if v1 < v2, 0 if equal, 1 if v1 > v2
    
    int major1 = 0, minor1 = 0, patch1 = 0;
    int major2 = 0, minor2 = 0, patch2 = 0;
    
    sscanf(v1, "v%d.%d.%d", &major1, &minor1, &patch1);
    sscanf(v2, "v%d.%d.%d", &major2, &minor2, &patch2);
    
    if (major1 != major2) return (major1 > major2) ? 1 : -1;
    if (minor1 != minor2) return (minor1 > minor2) ? 1 : -1;
    if (patch1 != patch2) return (patch1 > patch2) ? 1 : -1;
    
    return 0;
}

static esp_err_t check_for_update_impl(OTAManager_t *self, bool *update_available)
{
    if (!self) {
        ESP_LOGE(TAG, "OTA manager is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!update_available) {
        ESP_LOGE(TAG, "update_available pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!self->initialized) {
        ESP_LOGE(TAG, "OTA manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    *update_available = false;

    esp_err_t err;
    if (self->channel == OTA_CHANNEL_RELEASE) {
        err = self->fetch_latest_release(self);
    } else {
        err = self->fetch_testing_build(self);
    }

    if (err != ESP_OK) {
        return err;
    }

    // Compare versions
    if (self->channel == OTA_CHANNEL_RELEASE) {
        int cmp = compare_versions(self->current_version, self->latest_release.version);
        *update_available = (cmp < 0); // Update available if current < latest
        
        if (*update_available) {
            ESP_LOGI(TAG, "✓ Update available: %s → %s", 
                     self->current_version, self->latest_release.version);
        } else {
            ESP_LOGI(TAG, "Already on latest version: %s", self->current_version);
        }
    } else {
        // For testing channel, always consider update available
        *update_available = true;
        ESP_LOGI(TAG, "Testing build available");
    }

    return ESP_OK;
}

// ============================================================================
// Configuration Methods
// ============================================================================

static esp_err_t set_channel_impl(OTAManager_t *self, ota_channel_t channel)
{
    if (!self) {
        ESP_LOGE(TAG, "OTA manager is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!self->initialized) {
        ESP_LOGE(TAG, "OTA manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (channel != OTA_CHANNEL_RELEASE && channel != OTA_CHANNEL_TESTING) {
        ESP_LOGE(TAG, "Invalid channel: %d", channel);
        return ESP_ERR_INVALID_ARG;
    }

    self->channel = channel;
    ESP_LOGI(TAG, "Update channel set to: %s", 
             channel == OTA_CHANNEL_RELEASE ? "Release" : "Testing");
    return ESP_OK;
}

static esp_err_t set_auto_check_impl(OTAManager_t *self, bool enabled)
{
    if (!self->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    self->auto_check = enabled;
    ESP_LOGI(TAG, "Auto-check for updates: %s", enabled ? "enabled" : "disabled");
    return ESP_OK;
}

static esp_err_t get_current_version_impl(OTAManager_t *self, char *version_out)
{
    if (!self->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    strcpy(version_out, self->current_version);
    return ESP_OK;
}

static esp_err_t get_latest_version_impl(OTAManager_t *self, char *version_out)
{
    if (!self->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    strcpy(version_out, self->latest_release.version);
    return ESP_OK;
}

static void set_progress_callback_impl(OTAManager_t *self, ota_progress_callback_t callback)
{
    self->progress_callback = callback;
}

static const char* get_status_string_impl(OTAManager_t *self)
{
    switch (self->status) {
        case OTA_STATUS_IDLE: return "Idle";
        case OTA_STATUS_CHECKING: return "Checking for updates...";
        case OTA_STATUS_DOWNLOADING: return "Downloading firmware...";
        case OTA_STATUS_INSTALLING: return "Installing update...";
        case OTA_STATUS_SUCCESS: return "Update successful!";
        case OTA_STATUS_ERROR: return "Update failed";
        default: return "Unknown";
    }
}

// ============================================================================
// Utility Methods
// ============================================================================

static void print_info_impl(OTAManager_t *self)
{
    ESP_LOGI(TAG, "=== OTA Manager Info ===");
    ESP_LOGI(TAG, "Repository: %s/%s", self->github_owner, self->github_repo);
    ESP_LOGI(TAG, "Testing branch: %s", self->testing_branch);
    ESP_LOGI(TAG, "Current version: %s", self->current_version);
    ESP_LOGI(TAG, "Update channel: %s", 
             self->channel == OTA_CHANNEL_RELEASE ? "Release" : "Testing");
    ESP_LOGI(TAG, "Auto-check: %s", self->auto_check ? "enabled" : "disabled");
    ESP_LOGI(TAG, "Status: %s", self->get_status_string(self));
    
    if (strlen(self->latest_release.version) > 0) {
        ESP_LOGI(TAG, "Latest version: %s", self->latest_release.version);
        ESP_LOGI(TAG, "Download URL: %s", self->latest_release.download_url);
    }
}

// ============================================================================
// Initialization
// ============================================================================

static esp_err_t init_impl(OTAManager_t *self, const char *owner, const char *repo, const char *branch)
{
    if (!self) {
        ESP_LOGE(TAG, "OTA manager is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (self->initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    // Validate inputs
    if (!owner || strlen(owner) == 0 || strlen(owner) >= sizeof(self->github_owner)) {
        ESP_LOGE(TAG, "Invalid GitHub owner");
        return ESP_ERR_INVALID_ARG;
    }
    if (!repo || strlen(repo) == 0 || strlen(repo) >= sizeof(self->github_repo)) {
        ESP_LOGE(TAG, "Invalid GitHub repo");
        return ESP_ERR_INVALID_ARG;
    }
    if (!branch || strlen(branch) == 0 || strlen(branch) >= sizeof(self->testing_branch)) {
        ESP_LOGE(TAG, "Invalid branch name");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing OTA Manager...");

    // Safely copy repository information
    strncpy(self->github_owner, owner, sizeof(self->github_owner) - 1);
    self->github_owner[sizeof(self->github_owner) - 1] = '\0';
    strncpy(self->github_repo, repo, sizeof(self->github_repo) - 1);
    self->github_repo[sizeof(self->github_repo) - 1] = '\0';
    strncpy(self->testing_branch, branch, sizeof(self->testing_branch) - 1);
    self->testing_branch[sizeof(self->testing_branch) - 1] = '\0';

    // Set current firmware version
    strncpy(self->current_version, FIRMWARE_VERSION, sizeof(self->current_version) - 1);

    // Get running partition info
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
        ESP_LOGE(TAG, "Failed to get running partition");
        return ESP_FAIL;
    }
    
    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    
    ESP_LOGI(TAG, "Current partition: %s at 0x%08x", running->label, running->address);
    
    if (update) {
        ESP_LOGI(TAG, "Update partition: %s at 0x%08x", update->label, update->address);
    } else {
        ESP_LOGW(TAG, "No OTA update partition found (running from factory?)");
    }
    
    // Check if this is the first boot after an OTA update
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // This is first boot after OTA update - mark as valid
            ESP_LOGI(TAG, "First boot after OTA update detected");
            esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "✓ OTA update validated and confirmed");
            } else {
                ESP_LOGE(TAG, "Failed to validate OTA update: %s", esp_err_to_name(err));
            }
        } else if (ota_state == ESP_OTA_IMG_VALID) {
            ESP_LOGI(TAG, "Running validated firmware");
        } else if (ota_state == ESP_OTA_IMG_INVALID) {
            ESP_LOGW(TAG, "Running partition marked as invalid");
        } else if (ota_state == ESP_OTA_IMG_ABORTED) {
            ESP_LOGW(TAG, "Previous OTA update was aborted");
        }
    }

    self->status = OTA_STATUS_IDLE;
    self->initialized = true;

    ESP_LOGI(TAG, "✓ OTA Manager initialized");
    self->print_info(self);

    return ESP_OK;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

OTAManager_t *ota_manager_create(void)
{
    OTAManager_t *manager = calloc(1, sizeof(OTAManager_t));
    if (!manager) {
        ESP_LOGE(TAG, "Failed to allocate memory for OTA manager");
        return NULL;
    }

    // Initialize default values
    manager->channel = OTA_CHANNEL_RELEASE;
    manager->auto_check = false;
    manager->status = OTA_STATUS_IDLE;
    manager->initialized = false;
    manager->progress_callback = NULL;

    // Assign methods
    manager->init = init_impl;
    manager->check_for_update = check_for_update_impl;
    manager->perform_update = perform_update_impl;
    manager->set_channel = set_channel_impl;
    manager->set_auto_check = set_auto_check_impl;
    manager->get_current_version = get_current_version_impl;
    manager->get_latest_version = get_latest_version_impl;
    manager->set_progress_callback = set_progress_callback_impl;
    manager->get_status_string = get_status_string_impl;
    manager->fetch_latest_release = fetch_latest_release_impl;
    manager->fetch_testing_build = fetch_testing_build_impl;
    manager->download_and_install = download_and_install_impl;
    manager->print_info = print_info_impl;

    return manager;
}

void ota_manager_destroy(OTAManager_t *manager)
{
    if (manager) {
        free(manager);
    }
}
