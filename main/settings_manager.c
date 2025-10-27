#include "settings_manager.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

static const char *TAG = "SETTINGS";

// ============================================================================
// SPIFFS Initialization
// ============================================================================

static esp_err_t mount_spiffs(SettingsManager_t *self)
{
    if (self->spiffs_mounted)
    {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SPIFFS...");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "SPIFFS: %d KB total, %d KB used", total / 1024, used / 1024);
    }

    self->spiffs_mounted = true;
    return ESP_OK;
}

// ============================================================================
// File Helper Functions
// ============================================================================

static bool file_exists_impl(SettingsManager_t *self, const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0);
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        ESP_LOGW(TAG, "File not found: %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = malloc(fsize + 1);
    if (!content)
    {
        fclose(f);
        return NULL;
    }

    fread(content, 1, fsize, f);
    content[fsize] = '\0';
    fclose(f);

    return content;
}

static esp_err_t write_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "w");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
        return ESP_FAIL;
    }

    fprintf(f, "%s", content);
    fclose(f);
    return ESP_OK;
}

// ============================================================================
// Default Settings
// ============================================================================

static void set_default_settings(app_settings_t *settings)
{
    settings->camera_quality = DEFAULT_CAMERA_QUALITY;
    settings->camera_framesize = DEFAULT_CAMERA_FRAMESIZE;
    settings->camera_flip_h = false;
    settings->camera_flip_v = false;
    settings->camera_brightness = DEFAULT_CAMERA_BRIGHTNESS;
    settings->camera_contrast = DEFAULT_CAMERA_CONTRAST;
    settings->camera_saturation = DEFAULT_CAMERA_SATURATION;

    strncpy(settings->device_name, DEFAULT_DEVICE_NAME, sizeof(settings->device_name) - 1);
    settings->led_enabled = true;
    settings->http_port = DEFAULT_HTTP_PORT;

    strncpy(settings->server_upload_url, DEFAULT_SERVER_UPLOAD_URL, sizeof(settings->server_upload_url) - 1);
    settings->server_upload_enabled = DEFAULT_SERVER_UPLOAD_ENABLED;
    settings->server_upload_interval = DEFAULT_SERVER_UPLOAD_INTERVAL;
    settings->server_poll_interval = DEFAULT_SERVER_POLL_INTERVAL;

    settings->version = SETTINGS_VERSION;
}

static void set_default_secrets(secrets_t *secrets)
{
    memset(secrets->wifi_ssid, 0, sizeof(secrets->wifi_ssid));
    memset(secrets->wifi_password, 0, sizeof(secrets->wifi_password));
    secrets->configured = false;
}

// ============================================================================
// JSON Serialization/Deserialization
// ============================================================================

static esp_err_t settings_to_json(const app_settings_t *settings, char **json_str)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddNumberToObject(root, "version", settings->version);

    cJSON *camera = cJSON_CreateObject();
    cJSON_AddNumberToObject(camera, "quality", settings->camera_quality);
    cJSON_AddNumberToObject(camera, "framesize", settings->camera_framesize);
    cJSON_AddBoolToObject(camera, "flip_horizontal", settings->camera_flip_h);
    cJSON_AddBoolToObject(camera, "flip_vertical", settings->camera_flip_v);
    cJSON_AddNumberToObject(camera, "brightness", settings->camera_brightness);
    cJSON_AddNumberToObject(camera, "contrast", settings->camera_contrast);
    cJSON_AddNumberToObject(camera, "saturation", settings->camera_saturation);
    cJSON_AddItemToObject(root, "camera", camera);

    cJSON *system = cJSON_CreateObject();
    cJSON_AddStringToObject(system, "device_name", settings->device_name);
    cJSON_AddBoolToObject(system, "led_enabled", settings->led_enabled);
    cJSON_AddNumberToObject(system, "http_port", settings->http_port);
    cJSON_AddItemToObject(root, "system", system);

    cJSON *server = cJSON_CreateObject();
    cJSON_AddStringToObject(server, "upload_url", settings->server_upload_url);
    cJSON_AddBoolToObject(server, "upload_enabled", settings->server_upload_enabled);
    cJSON_AddNumberToObject(server, "upload_interval_seconds", settings->server_upload_interval);
    cJSON_AddNumberToObject(server, "poll_interval_ms", settings->server_poll_interval);
    cJSON_AddItemToObject(root, "server", server);

    *json_str = cJSON_Print(root);
    cJSON_Delete(root);

    return (*json_str != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t json_to_settings(const char *json_str, app_settings_t *settings)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root)
    {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_FAIL;
    }

    cJSON *version = cJSON_GetObjectItem(root, "version");
    if (version)
        settings->version = version->valueint;

    cJSON *camera = cJSON_GetObjectItem(root, "camera");
    if (camera)
    {
        cJSON *item;
        if ((item = cJSON_GetObjectItem(camera, "quality")))
            settings->camera_quality = item->valueint;
        if ((item = cJSON_GetObjectItem(camera, "framesize")))
            settings->camera_framesize = item->valueint;
        if ((item = cJSON_GetObjectItem(camera, "flip_horizontal")))
            settings->camera_flip_h = cJSON_IsTrue(item);
        if ((item = cJSON_GetObjectItem(camera, "flip_vertical")))
            settings->camera_flip_v = cJSON_IsTrue(item);
        if ((item = cJSON_GetObjectItem(camera, "brightness")))
            settings->camera_brightness = item->valueint;
        if ((item = cJSON_GetObjectItem(camera, "contrast")))
            settings->camera_contrast = item->valueint;
        if ((item = cJSON_GetObjectItem(camera, "saturation")))
            settings->camera_saturation = item->valueint;
    }

    cJSON *system = cJSON_GetObjectItem(root, "system");
    if (system)
    {
        cJSON *item;
        if ((item = cJSON_GetObjectItem(system, "device_name")))
            strncpy(settings->device_name, item->valuestring, sizeof(settings->device_name) - 1);
        if ((item = cJSON_GetObjectItem(system, "led_enabled")))
            settings->led_enabled = cJSON_IsTrue(item);
        if ((item = cJSON_GetObjectItem(system, "http_port")))
            settings->http_port = item->valueint;
    }

    cJSON *server = cJSON_GetObjectItem(root, "server");
    if (server)
    {
        cJSON *item;
        if ((item = cJSON_GetObjectItem(server, "upload_url")))
            strncpy(settings->server_upload_url, item->valuestring, sizeof(settings->server_upload_url) - 1);
        if ((item = cJSON_GetObjectItem(server, "upload_enabled")))
            settings->server_upload_enabled = cJSON_IsTrue(item);
        if ((item = cJSON_GetObjectItem(server, "upload_interval_seconds")))
            settings->server_upload_interval = item->valueint;
        if ((item = cJSON_GetObjectItem(server, "poll_interval_ms")))
            settings->server_poll_interval = item->valueint;
    }

    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t secrets_to_json(const secrets_t *secrets, char **json_str)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        return ESP_ERR_NO_MEM;
    }

    cJSON *wifi = cJSON_CreateObject();
    cJSON_AddStringToObject(wifi, "ssid", secrets->wifi_ssid);
    cJSON_AddStringToObject(wifi, "password", secrets->wifi_password);
    cJSON_AddBoolToObject(wifi, "configured", secrets->configured);
    cJSON_AddItemToObject(root, "wifi", wifi);

    *json_str = cJSON_Print(root);
    cJSON_Delete(root);

    return (*json_str != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t json_to_secrets(const char *json_str, secrets_t *secrets)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root)
    {
        ESP_LOGE(TAG, "Failed to parse secrets JSON");
        return ESP_FAIL;
    }

    cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
    if (wifi)
    {
        cJSON *item;
        if ((item = cJSON_GetObjectItem(wifi, "ssid")))
            strncpy(secrets->wifi_ssid, item->valuestring, sizeof(secrets->wifi_ssid) - 1);
        if ((item = cJSON_GetObjectItem(wifi, "password")))
            strncpy(secrets->wifi_password, item->valuestring, sizeof(secrets->wifi_password) - 1);
        if ((item = cJSON_GetObjectItem(wifi, "configured")))
            secrets->configured = cJSON_IsTrue(item);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

// ============================================================================
// Settings Manager Methods
// ============================================================================

static esp_err_t init_impl(SettingsManager_t *self)
{
    ESP_LOGI(TAG, "Initializing settings manager...");

    esp_err_t ret = mount_spiffs(self);
    if (ret != ESP_OK)
    {
        return ret;
    }

    // Load or create default settings
    ret = self->load_settings(self);
    if (ret != ESP_OK)
    {
        ESP_LOGI(TAG, "Creating default settings file...");
        set_default_settings(&self->settings);
        self->save_settings(self);
    }

    // Load or create default secrets
    ret = self->load_secrets(self);
    if (ret != ESP_OK)
    {
        ESP_LOGI(TAG, "Creating default secrets file...");
        set_default_secrets(&self->secrets);
        self->save_secrets(self);
    }

    self->initialized = true;
    ESP_LOGI(TAG, "Settings manager initialized");
    return ESP_OK;
}

static esp_err_t load_settings_impl(SettingsManager_t *self)
{
    char *json_str = read_file(SETTINGS_FILE_PATH);
    if (!json_str)
    {
        return ESP_FAIL;
    }

    esp_err_t ret = json_to_settings(json_str, &self->settings);
    free(json_str);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Settings loaded from %s", SETTINGS_FILE_PATH);
    }
    return ret;
}

static esp_err_t save_settings_impl(SettingsManager_t *self)
{
    char *json_str = NULL;
    esp_err_t ret = settings_to_json(&self->settings, &json_str);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = write_file(SETTINGS_FILE_PATH, json_str);
    free(json_str);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Settings saved to %s", SETTINGS_FILE_PATH);
    }
    return ret;
}

static esp_err_t load_secrets_impl(SettingsManager_t *self)
{
    char *json_str = read_file(SECRETS_FILE_PATH);
    if (!json_str)
    {
        return ESP_FAIL;
    }

    esp_err_t ret = json_to_secrets(json_str, &self->secrets);
    free(json_str);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Secrets loaded from %s", SECRETS_FILE_PATH);
    }
    return ret;
}

static esp_err_t save_secrets_impl(SettingsManager_t *self)
{
    char *json_str = NULL;
    esp_err_t ret = secrets_to_json(&self->secrets, &json_str);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = write_file(SECRETS_FILE_PATH, json_str);
    free(json_str);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Secrets saved to %s", SECRETS_FILE_PATH);
    }
    return ret;
}

static esp_err_t reset_to_defaults_impl(SettingsManager_t *self)
{
    ESP_LOGI(TAG, "Resetting settings to defaults...");
    set_default_settings(&self->settings);
    return self->save_settings(self);
}

static esp_err_t clear_all_impl(SettingsManager_t *self)
{
    ESP_LOGI(TAG, "Clearing all settings and secrets...");

    remove(SETTINGS_FILE_PATH);
    remove(SECRETS_FILE_PATH);

    set_default_settings(&self->settings);
    set_default_secrets(&self->secrets);

    return ESP_OK;
}

// WiFi credentials methods
static esp_err_t set_wifi_credentials_impl(SettingsManager_t *self,
                                           const char *ssid, const char *password)
{
    if (!ssid || !password)
    {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(self->secrets.wifi_ssid, ssid, sizeof(self->secrets.wifi_ssid) - 1);
    strncpy(self->secrets.wifi_password, password, sizeof(self->secrets.wifi_password) - 1);
    self->secrets.configured = true;

    return self->save_secrets(self);
}

static esp_err_t get_wifi_credentials_impl(SettingsManager_t *self,
                                           char *ssid, char *password)
{
    if (!ssid || !password)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!self->secrets.configured)
    {
        return ESP_ERR_NOT_FOUND;
    }

    strcpy(ssid, self->secrets.wifi_ssid);
    strcpy(password, self->secrets.wifi_password);
    return ESP_OK;
}

static bool has_wifi_credentials_impl(SettingsManager_t *self)
{
    return self->secrets.configured && (strlen(self->secrets.wifi_ssid) > 0);
}

// Camera settings methods
static esp_err_t set_camera_quality_impl(SettingsManager_t *self, uint8_t quality)
{
    if (quality > 63)
    {
        return ESP_ERR_INVALID_ARG;
    }
    self->settings.camera_quality = quality;
    return self->save_settings(self);
}

static esp_err_t set_camera_framesize_impl(SettingsManager_t *self, uint16_t framesize)
{
    self->settings.camera_framesize = framesize;
    return self->save_settings(self);
}

static esp_err_t set_camera_flip_impl(SettingsManager_t *self, bool h_flip, bool v_flip)
{
    self->settings.camera_flip_h = h_flip;
    self->settings.camera_flip_v = v_flip;
    return self->save_settings(self);
}

static esp_err_t set_camera_brightness_impl(SettingsManager_t *self, int8_t brightness)
{
    if (brightness < -2 || brightness > 2)
    {
        return ESP_ERR_INVALID_ARG;
    }
    self->settings.camera_brightness = brightness;
    return self->save_settings(self);
}

// System settings methods
static esp_err_t set_device_name_impl(SettingsManager_t *self, const char *name)
{
    if (!name)
    {
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(self->settings.device_name, name, sizeof(self->settings.device_name) - 1);
    return self->save_settings(self);
}

static esp_err_t set_led_enabled_impl(SettingsManager_t *self, bool enabled)
{
    self->settings.led_enabled = enabled;
    return self->save_settings(self);
}

// Server settings methods
static esp_err_t set_server_upload_url_impl(SettingsManager_t *self, const char *url)
{
    if (!url)
    {
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(self->settings.server_upload_url, url, sizeof(self->settings.server_upload_url) - 1);
    return self->save_settings(self);
}

static esp_err_t set_server_upload_enabled_impl(SettingsManager_t *self, bool enabled)
{
    self->settings.server_upload_enabled = enabled;
    return self->save_settings(self);
}

static esp_err_t set_server_upload_interval_impl(SettingsManager_t *self, uint32_t interval)
{
    if (interval < 5) // Minimum 5 seconds
    {
        return ESP_ERR_INVALID_ARG;
    }
    self->settings.server_upload_interval = interval;
    return self->save_settings(self);
}

static esp_err_t set_server_poll_interval_impl(SettingsManager_t *self, uint32_t interval_ms)
{
    if (interval_ms < 50) // Minimum 50ms to avoid overwhelming server
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (interval_ms > 10000) // Maximum 10 seconds
    {
        return ESP_ERR_INVALID_ARG;
    }
    self->settings.server_poll_interval = interval_ms;
    return self->save_settings(self);
}

// Export/Import methods
static esp_err_t export_settings_json_impl(SettingsManager_t *self, char **json_str)
{
    return settings_to_json(&self->settings, json_str);
}

static esp_err_t import_settings_json_impl(SettingsManager_t *self, const char *json_str)
{
    esp_err_t ret = json_to_settings(json_str, &self->settings);
    if (ret == ESP_OK)
    {
        ret = self->save_settings(self);
    }
    return ret;
}

// Print settings
static void print_impl(SettingsManager_t *self)
{
    ESP_LOGI(TAG, "=== Current Settings ===");
    ESP_LOGI(TAG, "Version: %lu", self->settings.version);
    ESP_LOGI(TAG, "Device Name: %s", self->settings.device_name);
    ESP_LOGI(TAG, "HTTP Port: %d", self->settings.http_port);
    ESP_LOGI(TAG, "LED Enabled: %s", self->settings.led_enabled ? "Yes" : "No");
    ESP_LOGI(TAG, "Camera Quality: %d", self->settings.camera_quality);
    ESP_LOGI(TAG, "Camera Framesize: %d", self->settings.camera_framesize);
    ESP_LOGI(TAG, "Camera Flip H/V: %s/%s",
             self->settings.camera_flip_h ? "Yes" : "No",
             self->settings.camera_flip_v ? "Yes" : "No");
    ESP_LOGI(TAG, "Camera Brightness: %d", self->settings.camera_brightness);
    ESP_LOGI(TAG, "Camera Contrast: %d", self->settings.camera_contrast);
    ESP_LOGI(TAG, "Camera Saturation: %d", self->settings.camera_saturation);
    ESP_LOGI(TAG, "Server Upload URL: %s", self->settings.server_upload_url);
    ESP_LOGI(TAG, "Server Upload Enabled: %s", self->settings.server_upload_enabled ? "Yes" : "No");
    ESP_LOGI(TAG, "Server Upload Interval: %lu seconds", self->settings.server_upload_interval);
    ESP_LOGI(TAG, "Server Poll Interval: %lu ms", self->settings.server_poll_interval);
    ESP_LOGI(TAG, "WiFi Configured: %s", self->secrets.configured ? "Yes" : "No");
    if (self->secrets.configured)
    {
        ESP_LOGI(TAG, "WiFi SSID: %s", self->secrets.wifi_ssid);
    }
}

// ============================================================================
// Constructor/Destructor
// ============================================================================

SettingsManager_t *settings_manager_create(void)
{
    SettingsManager_t *manager = malloc(sizeof(SettingsManager_t));
    if (!manager)
    {
        ESP_LOGE(TAG, "Failed to allocate memory");
        return NULL;
    }

    memset(manager, 0, sizeof(SettingsManager_t));

    // Set default values
    set_default_settings(&manager->settings);
    set_default_secrets(&manager->secrets);

    // Assign methods
    manager->init = init_impl;
    manager->load_settings = load_settings_impl;
    manager->save_settings = save_settings_impl;
    manager->load_secrets = load_secrets_impl;
    manager->save_secrets = save_secrets_impl;
    manager->reset_to_defaults = reset_to_defaults_impl;
    manager->clear_all = clear_all_impl;

    manager->set_wifi_credentials = set_wifi_credentials_impl;
    manager->get_wifi_credentials = get_wifi_credentials_impl;
    manager->has_wifi_credentials = has_wifi_credentials_impl;

    manager->set_camera_quality = set_camera_quality_impl;
    manager->set_camera_framesize = set_camera_framesize_impl;
    manager->set_camera_flip = set_camera_flip_impl;
    manager->set_camera_brightness = set_camera_brightness_impl;

    manager->set_device_name = set_device_name_impl;
    manager->set_led_enabled = set_led_enabled_impl;

    manager->set_server_upload_url = set_server_upload_url_impl;
    manager->set_server_upload_enabled = set_server_upload_enabled_impl;
    manager->set_server_upload_interval = set_server_upload_interval_impl;
    manager->set_server_poll_interval = set_server_poll_interval_impl;

    manager->export_settings_json = export_settings_json_impl;
    manager->import_settings_json = import_settings_json_impl;

    manager->print = print_impl;
    manager->file_exists = file_exists_impl;

    return manager;
}

void settings_manager_destroy(SettingsManager_t *manager)
{
    if (manager)
    {
        if (manager->spiffs_mounted)
        {
            esp_vfs_spiffs_unregister(NULL);
        }
        free(manager);
    }
}
