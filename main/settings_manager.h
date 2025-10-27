#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// Secrets structure - sensitive data stored separately
typedef struct
{
    char wifi_ssid[32];
    char wifi_password[64];
    bool configured;
} secrets_t;

// Settings structure - general configuration
typedef struct
{
    // Camera settings
    uint8_t camera_quality;    // JPEG quality (0-63, lower = higher quality)
    uint16_t camera_framesize; // Frame size (e.g., FRAMESIZE_VGA)
    bool camera_flip_h;        // Horizontal flip
    bool camera_flip_v;        // Vertical flip
    int8_t camera_brightness;  // -2 to 2
    int8_t camera_contrast;    // -2 to 2
    int8_t camera_saturation;  // -2 to 2

    // System settings
    char device_name[32]; // Device hostname
    bool led_enabled;     // Enable/disable status LED
    uint16_t http_port;   // Web server port

    // Server settings
    char server_upload_url[256];     // Flask server upload URL
    bool server_upload_enabled;      // Enable/disable auto upload
    uint32_t server_upload_interval; // Upload interval in seconds
    uint32_t server_poll_interval;   // Polling interval in milliseconds

    // Version
    uint32_t version; // Settings version
} app_settings_t;

// Settings Manager "Class"
typedef struct SettingsManager_t
{
    app_settings_t settings;
    secrets_t secrets;
    bool initialized;
    bool spiffs_mounted;

    // Methods
    esp_err_t (*init)(struct SettingsManager_t *self);
    esp_err_t (*load_settings)(struct SettingsManager_t *self);
    esp_err_t (*save_settings)(struct SettingsManager_t *self);
    esp_err_t (*load_secrets)(struct SettingsManager_t *self);
    esp_err_t (*save_secrets)(struct SettingsManager_t *self);
    esp_err_t (*reset_to_defaults)(struct SettingsManager_t *self);
    esp_err_t (*clear_all)(struct SettingsManager_t *self);

    // WiFi settings (secrets)
    esp_err_t (*set_wifi_credentials)(struct SettingsManager_t *self,
                                      const char *ssid, const char *password);
    esp_err_t (*get_wifi_credentials)(struct SettingsManager_t *self,
                                      char *ssid, char *password);
    bool (*has_wifi_credentials)(struct SettingsManager_t *self);

    // Camera settings
    esp_err_t (*set_camera_quality)(struct SettingsManager_t *self, uint8_t quality);
    esp_err_t (*set_camera_framesize)(struct SettingsManager_t *self, uint16_t framesize);
    esp_err_t (*set_camera_flip)(struct SettingsManager_t *self, bool h_flip, bool v_flip);
    esp_err_t (*set_camera_brightness)(struct SettingsManager_t *self, int8_t brightness);

    // System settings
    esp_err_t (*set_device_name)(struct SettingsManager_t *self, const char *name);
    esp_err_t (*set_led_enabled)(struct SettingsManager_t *self, bool enabled);

    // Server settings
    esp_err_t (*set_server_upload_url)(struct SettingsManager_t *self, const char *url);
    esp_err_t (*set_server_upload_enabled)(struct SettingsManager_t *self, bool enabled);
    esp_err_t (*set_server_upload_interval)(struct SettingsManager_t *self, uint32_t interval);
    esp_err_t (*set_server_poll_interval)(struct SettingsManager_t *self, uint32_t interval_ms);

    // File operations
    esp_err_t (*export_settings_json)(struct SettingsManager_t *self, char **json_str);
    esp_err_t (*import_settings_json)(struct SettingsManager_t *self, const char *json_str);

    // Utility
    void (*print)(struct SettingsManager_t *self);
    bool (*file_exists)(struct SettingsManager_t *self, const char *path);
} SettingsManager_t;

// Constructor
SettingsManager_t *settings_manager_create(void);

// Destructor
void settings_manager_destroy(SettingsManager_t *manager);

// File paths
#define SETTINGS_FILE_PATH "/spiffs/settings.json"
#define SECRETS_FILE_PATH "/spiffs/secrets.json"

// Default values
#define SETTINGS_VERSION 1
#define DEFAULT_DEVICE_NAME "ESP32-Camera"
#define DEFAULT_HTTP_PORT 80
#define DEFAULT_CAMERA_QUALITY 12
#define DEFAULT_CAMERA_FRAMESIZE 10 // FRAMESIZE_SVGA
#define DEFAULT_CAMERA_BRIGHTNESS 0
#define DEFAULT_CAMERA_CONTRAST 0
#define DEFAULT_CAMERA_SATURATION 0
#define DEFAULT_SERVER_UPLOAD_URL "http://192.168.1.100:5000/api/capture"
#define DEFAULT_SERVER_UPLOAD_ENABLED true
#define DEFAULT_SERVER_UPLOAD_INTERVAL 30
#define DEFAULT_SERVER_POLL_INTERVAL 500

#endif // SETTINGS_MANAGER_H
