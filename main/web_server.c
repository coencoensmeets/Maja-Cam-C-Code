#include "web_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "main_menu.h"
#include "ota_manager.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "WEBSERVER";

// Global references for HTTP handlers (C limitation workaround)
static Camera_t *g_camera = NULL;
static HttpClient_t *g_http_client = NULL;
static SettingsManager_t *g_settings = NULL;
static LEDRing_t *g_led_ring = NULL;
static ThermalPrinter_t *g_printer = NULL;

// External OTA manager from main.c
extern OTAManager_t *g_ota_manager;

// HTTP handler for index page
static esp_err_t webserver_index_handler(httpd_req_t *req)
{
    const char *html =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<title>ESP32-S3 Camera</title>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>"
        "body { font-family: Arial; text-align: center; margin: 20px; background: #1a1a1a; color: white; }"
        "h1 { color: #4CAF50; }"
        "img { max-width: 90%; height: auto; border: 3px solid #4CAF50; border-radius: 10px; margin: 20px 0; }"
        "button { background-color: #4CAF50; color: white; padding: 15px 32px; font-size: 16px; "
        "border: none; border-radius: 5px; cursor: pointer; margin: 10px; }"
        "button:hover { background-color: #45a049; }"
        ".info { background: #2a2a2a; padding: 15px; border-radius: 10px; margin: 20px auto; max-width: 600px; }"
        "</style>"
        "</head>"
        "<body>"
        "<h1>ESP32-S3 OV2640 Camera</h1>"
        "<div class='info'>"
        "<p>Camera: OV2640 | Resolution: VGA (640x480)</p>"
        "<p>Object-Oriented C Implementation</p>"
        "</div>"
        "<img id='stream' src='/capture' />"
        "<br>"
        "<button onclick='location.reload()'>Refresh Image</button>"
        "<button onclick='startAutoRefresh()' id='autoBtn'>Auto Refresh (5s)</button>"
        "<script>"
        "let autoRefresh = null;"
        "function startAutoRefresh() {"
        "  if (autoRefresh) {"
        "    clearInterval(autoRefresh);"
        "    autoRefresh = null;"
        "    document.getElementById('autoBtn').textContent = 'Auto Refresh (5s)';"
        "  } else {"
        "    autoRefresh = setInterval(() => {"
        "      document.getElementById('stream').src = '/capture?t=' + new Date().getTime();"
        "    }, 5000);"
        "    document.getElementById('autoBtn').textContent = 'Stop Auto Refresh';"
        "  }"
        "}"
        "</script>"
        "</body>"
        "</html>";

    ESP_LOGI(TAG, "Index request received");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    esp_err_t res = httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);

    if (res == ESP_OK)
    {
        ESP_LOGI(TAG, "HTML page sent successfully");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to send HTML, error: 0x%x", res);
    }

    return res;
}

// HTTP handler for image capture
static esp_err_t webserver_capture_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Capture request received");

    if (!g_camera)
    {
        ESP_LOGE(TAG, "Camera object is NULL!");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    if (!g_camera->initialized)
    {
        ESP_LOGE(TAG, "Camera not initialized!");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Capturing image...");
    camera_fb_t *fb = g_camera->capture(g_camera);
    if (!fb)
    {
        ESP_LOGE(TAG, "Failed to capture image");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Image captured: %zu bytes", fb->len);

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);

    g_camera->return_frame(g_camera, fb);

    if (res == ESP_OK)
    {
        ESP_LOGI(TAG, "Image sent successfully");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to send image, error: 0x%x", res);
    }

    return res;
}

// HTTP handler for triggering capture and upload
static esp_err_t webserver_trigger_capture_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Trigger capture request received");

    if (!g_http_client)
    {
        ESP_LOGE(TAG, "HTTP client is NULL!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "HTTP client not initialized");
        return ESP_FAIL;
    }

    // Trigger capture and upload
    esp_err_t result = g_http_client->capture_and_upload(g_http_client);

    if (result == ESP_OK)
    {
        const char *response = "{\"status\":\"success\",\"message\":\"Image captured and uploaded\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(TAG, "Trigger capture successful");
        return ESP_OK;
    }
    else
    {
        const char *response = "{\"status\":\"error\",\"message\":\"Failed to capture or upload image\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, response);
        ESP_LOGE(TAG, "Trigger capture failed");
        return ESP_FAIL;
    }
}

// HTTP handler for getting settings
static esp_err_t webserver_get_settings_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Get settings request received");

    if (!g_settings)
    {
        ESP_LOGE(TAG, "Settings manager is NULL!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Settings not initialized");
        return ESP_FAIL;
    }

    // Create JSON object with all settings
    cJSON *root = cJSON_CreateObject();

    // Firmware version
    cJSON_AddStringToObject(root, "firmware_version", FIRMWARE_VERSION);

    // Camera settings
    cJSON *camera = cJSON_CreateObject();
    cJSON_AddNumberToObject(camera, "framesize", g_settings->settings.camera_framesize);
    cJSON_AddNumberToObject(camera, "quality", g_settings->settings.camera_quality);
    cJSON_AddNumberToObject(camera, "brightness", g_settings->settings.camera_brightness);
    cJSON_AddNumberToObject(camera, "contrast", g_settings->settings.camera_contrast);
    cJSON_AddNumberToObject(camera, "saturation", g_settings->settings.camera_saturation);
    cJSON_AddBoolToObject(camera, "vflip", g_settings->settings.camera_flip_v);
    cJSON_AddBoolToObject(camera, "hmirror", g_settings->settings.camera_flip_h);
    cJSON_AddItemToObject(root, "camera", camera);

    // LED Ring settings
    cJSON *led_ring = cJSON_CreateObject();
    cJSON_AddNumberToObject(led_ring, "brightness", g_settings->settings.led_ring_brightness);
    cJSON_AddNumberToObject(led_ring, "count", g_settings->settings.led_ring_count);
    cJSON *color = cJSON_CreateObject();
    cJSON_AddNumberToObject(color, "r", g_settings->settings.led_ring_red);
    cJSON_AddNumberToObject(color, "g", g_settings->settings.led_ring_green);
    cJSON_AddNumberToObject(color, "b", g_settings->settings.led_ring_blue);
    cJSON_AddItemToObject(led_ring, "color", color);
    cJSON_AddItemToObject(root, "led_ring", led_ring);

    // Server settings
    cJSON *server = cJSON_CreateObject();
    cJSON_AddStringToObject(server, "upload_url", g_settings->settings.server_upload_url);
    cJSON_AddBoolToObject(server, "upload_enabled", g_settings->settings.server_upload_enabled);
    cJSON_AddNumberToObject(server, "upload_interval_seconds", g_settings->settings.server_upload_interval);
    cJSON_AddItemToObject(root, "server", server);

    // OTA settings
    cJSON *ota = cJSON_CreateObject();
    cJSON_AddNumberToObject(ota, "update_channel", g_settings->settings.ota_update_channel);
    cJSON_AddBoolToObject(ota, "auto_check", g_settings->settings.ota_auto_check);
    cJSON_AddStringToObject(ota, "github_owner", g_settings->settings.ota_github_owner);
    cJSON_AddStringToObject(ota, "github_repo", g_settings->settings.ota_github_repo);
    cJSON_AddStringToObject(ota, "testing_branch", g_settings->settings.ota_testing_branch);
    cJSON_AddItemToObject(root, "ota", ota);

    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);

    if (json_string)
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        esp_err_t res = httpd_resp_send(req, json_string, HTTPD_RESP_USE_STRLEN);
        free(json_string);
        ESP_LOGI(TAG, "Settings sent successfully");
        return res;
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create JSON");
        return ESP_FAIL;
    }
}

// HTTP handler for updating settings
static esp_err_t webserver_update_settings_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Update settings request received");

    if (!g_settings)
    {
        ESP_LOGE(TAG, "Settings manager is NULL!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Settings not initialized");
        return ESP_FAIL;
    }

    // Read request body
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
    {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    ESP_LOGI(TAG, "Received settings JSON: %s", buf);

    // Parse JSON
    cJSON *root = cJSON_Parse(buf);
    if (!root)
    {
        ESP_LOGE(TAG, "Failed to parse JSON");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    bool success = true;

    // Update camera settings
    cJSON *camera = cJSON_GetObjectItem(root, "camera");
    if (camera)
    {
        cJSON *framesize = cJSON_GetObjectItem(camera, "framesize");
        if (framesize && cJSON_IsNumber(framesize))
        {
            g_settings->set_camera_framesize(g_settings, framesize->valueint);
        }

        cJSON *quality = cJSON_GetObjectItem(camera, "quality");
        if (quality && cJSON_IsNumber(quality))
        {
            g_settings->set_camera_quality(g_settings, quality->valueint);
        }

        cJSON *brightness = cJSON_GetObjectItem(camera, "brightness");
        if (brightness && cJSON_IsNumber(brightness))
        {
            g_settings->set_camera_brightness(g_settings, brightness->valueint);
        }

        cJSON *contrast = cJSON_GetObjectItem(camera, "contrast");
        if (contrast && cJSON_IsNumber(contrast))
        {
            g_settings->set_camera_contrast(g_settings, contrast->valueint);
        }

        cJSON *saturation = cJSON_GetObjectItem(camera, "saturation");
        if (saturation && cJSON_IsNumber(saturation))
        {
            g_settings->set_camera_saturation(g_settings, saturation->valueint);
        }

        cJSON *vflip = cJSON_GetObjectItem(camera, "vflip");
        if (vflip && cJSON_IsBool(vflip))
        {
            bool flip_v = cJSON_IsTrue(vflip);
            g_settings->set_camera_flip(g_settings, g_settings->settings.camera_flip_h, flip_v);
        }

        cJSON *hmirror = cJSON_GetObjectItem(camera, "hmirror");
        if (hmirror && cJSON_IsBool(hmirror))
        {
            bool flip_h = cJSON_IsTrue(hmirror);
            g_settings->set_camera_flip(g_settings, flip_h, g_settings->settings.camera_flip_v);
        }
    }

    // Update LED ring settings
    cJSON *led_ring = cJSON_GetObjectItem(root, "led_ring");
    if (led_ring)
    {
        cJSON *brightness = cJSON_GetObjectItem(led_ring, "brightness");
        if (brightness && cJSON_IsNumber(brightness))
        {
            uint8_t new_brightness = brightness->valueint;
            g_settings->set_led_ring_brightness(g_settings, new_brightness);
            
            // Also update the LED ring brightness immediately if available
            if (g_led_ring)
            {
                g_led_ring->set_brightness(g_led_ring, new_brightness);
                // Refresh the menu display to show new brightness
                refresh_led_ring_menu();
                ESP_LOGI(TAG, "LED ring brightness updated to %d%% and menu refreshed", new_brightness);
            }
        }

        cJSON *count = cJSON_GetObjectItem(led_ring, "count");
        if (count && cJSON_IsNumber(count))
        {
            g_settings->set_led_ring_count(g_settings, count->valueint);
            ESP_LOGI(TAG, "LED ring count updated - restart required to take effect");
        }
    }

    // Update server settings
    cJSON *server = cJSON_GetObjectItem(root, "server");
    if (server)
    {
        cJSON *upload_url = cJSON_GetObjectItem(server, "upload_url");
        if (upload_url && cJSON_IsString(upload_url))
        {
            g_settings->set_server_upload_url(g_settings, upload_url->valuestring);
        }

        cJSON *upload_enabled = cJSON_GetObjectItem(server, "upload_enabled");
        if (upload_enabled && cJSON_IsBool(upload_enabled))
        {
            g_settings->set_server_upload_enabled(g_settings, cJSON_IsTrue(upload_enabled));
        }

        cJSON *upload_interval = cJSON_GetObjectItem(server, "upload_interval_seconds");
        if (upload_interval && cJSON_IsNumber(upload_interval))
        {
            g_settings->set_server_upload_interval(g_settings, upload_interval->valueint);
        }
    }

    // Update OTA settings
    cJSON *ota = cJSON_GetObjectItem(root, "ota");
    if (ota)
    {
        cJSON *update_channel = cJSON_GetObjectItem(ota, "update_channel");
        if (update_channel && cJSON_IsNumber(update_channel))
        {
            g_settings->settings.ota_update_channel = update_channel->valueint;
            ESP_LOGI(TAG, "OTA update channel set to: %d", update_channel->valueint);
        }

        cJSON *auto_check = cJSON_GetObjectItem(ota, "auto_check");
        if (auto_check && cJSON_IsBool(auto_check))
        {
            g_settings->settings.ota_auto_check = cJSON_IsTrue(auto_check);
            ESP_LOGI(TAG, "OTA auto-check set to: %d", g_settings->settings.ota_auto_check);
        }
    }

    cJSON_Delete(root);

    // Save settings to SPIFFS
    if (g_settings->save_settings(g_settings) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save settings");
        success = false;
    }

    // Send response
    const char *response = success ? "{\"status\":\"success\",\"message\":\"Settings updated\"}" : "{\"status\":\"error\",\"message\":\"Failed to save settings\"}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if (success)
    {
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(TAG, "Settings updated successfully");
        return ESP_OK;
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, response);
        return ESP_FAIL;
    }
}

// HTTP handler for printing poems
static esp_err_t webserver_print_poem_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Print poem request received");
    
    // Check if printer is available
    if (!g_printer || !g_printer->initialized) {
        ESP_LOGW(TAG, "Thermal printer not initialized");
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, "{\"status\":\"error\",\"message\":\"Printer not available\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    // Read POST data
    char content[2048] = {0};
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read request body");
        return ESP_FAIL;
    }
    
    // Parse JSON
    cJSON *root = cJSON_Parse(content);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    // Extract fields
    cJSON *title_json = cJSON_GetObjectItem(root, "title");
    cJSON *poet_style_json = cJSON_GetObjectItem(root, "poet_style");
    cJSON *poem_text_json = cJSON_GetObjectItem(root, "poem_text");
    
    const char *title = title_json && cJSON_IsString(title_json) ? title_json->valuestring : "Untitled";
    const char *poet_style = poet_style_json && cJSON_IsString(poet_style_json) ? poet_style_json->valuestring : "General";
    const char *poem_text = poem_text_json && cJSON_IsString(poem_text_json) ? poem_text_json->valuestring : "";
    
    ESP_LOGI(TAG, "Printing poem: %s (%s)", title, poet_style);
    
    // Print the poem
    esp_err_t result = thermal_printer_print_poem(g_printer, title, poet_style, poem_text);
    
    cJSON_Delete(root);
    
    // Send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    if (result == ESP_OK) {
        httpd_resp_send(req, "{\"status\":\"success\",\"message\":\"Poem printed successfully\"}", HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(TAG, "Poem printed successfully");
        return ESP_OK;
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"status\":\"error\",\"message\":\"Failed to print poem\"}");
        return ESP_FAIL;
    }
}

// HTTP handler for OTA update check
static esp_err_t webserver_ota_check_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "OTA check request received");
    
    if (!g_ota_manager) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"error\":\"OTA manager not initialized\"}");
        return ESP_FAIL;
    }
    
    bool update_available = false;
    esp_err_t err = g_ota_manager->check_for_update(g_ota_manager, &update_available);
    
    cJSON *root = cJSON_CreateObject();
    
    if (err == ESP_OK) {
        char current[32], latest[32];
        g_ota_manager->get_current_version(g_ota_manager, current);
        g_ota_manager->get_latest_version(g_ota_manager, latest);
        
        cJSON_AddBoolToObject(root, "success", true);
        cJSON_AddBoolToObject(root, "update_available", update_available);
        cJSON_AddStringToObject(root, "current_version", current);
        cJSON_AddStringToObject(root, "latest_version", latest);
        cJSON_AddStringToObject(root, "channel", 
            g_settings->settings.ota_update_channel == 0 ? "release" : "testing");
    } else if (err == ESP_ERR_INVALID_STATE) {
        // Rate limited
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "rate_limited");
        cJSON_AddStringToObject(root, "message", "Please wait before checking again");
    } else {
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "check_failed");
        cJSON_AddStringToObject(root, "message", "Failed to check for updates");
    }
    
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, json_str);
    
    free(json_str);
    return ESP_OK;
}

// HTTP handler for OTA update trigger
static esp_err_t webserver_ota_update_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "OTA update request received");
    
    if (!g_ota_manager) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"error\":\"OTA manager not initialized\"}");
        return ESP_FAIL;
    }
    
    esp_err_t err = g_ota_manager->perform_update(g_ota_manager);
    
    cJSON *root = cJSON_CreateObject();
    
    if (err == ESP_OK) {
        cJSON_AddBoolToObject(root, "success", true);
        cJSON_AddStringToObject(root, "message", "Update successful! Device will restart...");
    } else {
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "update_failed");
        cJSON_AddStringToObject(root, "message", "Failed to perform update");
    }
    
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, json_str);
    
    free(json_str);
    return ESP_OK;
}

static esp_err_t webserver_start_impl(WebServer_t *self)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.stack_size = 8192;

    if (httpd_start(&self->server, &config) == ESP_OK)
    {
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = webserver_index_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(self->server, &index_uri);

        httpd_uri_t capture_uri = {
            .uri = "/capture",
            .method = HTTP_GET,
            .handler = webserver_capture_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(self->server, &capture_uri);

        httpd_uri_t trigger_capture_uri = {
            .uri = "/trigger-capture",
            .method = HTTP_GET,
            .handler = webserver_trigger_capture_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(self->server, &trigger_capture_uri);

        httpd_uri_t get_settings_uri = {
            .uri = "/api/settings",
            .method = HTTP_GET,
            .handler = webserver_get_settings_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(self->server, &get_settings_uri);

        httpd_uri_t update_settings_uri = {
            .uri = "/api/settings",
            .method = HTTP_POST,
            .handler = webserver_update_settings_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(self->server, &update_settings_uri);

        httpd_uri_t print_poem_uri = {
            .uri = "/api/print-poem",
            .method = HTTP_POST,
            .handler = webserver_print_poem_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(self->server, &print_poem_uri);

        httpd_uri_t ota_check_uri = {
            .uri = "/api/ota/check",
            .method = HTTP_GET,
            .handler = webserver_ota_check_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(self->server, &ota_check_uri);

        httpd_uri_t ota_update_uri = {
            .uri = "/api/ota/update",
            .method = HTTP_POST,
            .handler = webserver_ota_update_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(self->server, &ota_update_uri);

        self->running = true;
        ESP_LOGI(TAG, "Web server started successfully!");
        ESP_LOGI(TAG, "Visit http://<ESP32-IP>/ in your browser");
        ESP_LOGI(TAG, "API endpoints: /trigger-capture, /api/settings, /api/print-poem, /api/ota/check, /api/ota/update");
        return ESP_OK;
    }

    return ESP_FAIL;
}

static void webserver_stop_impl(WebServer_t *self)
{
    if (self->server)
    {
        httpd_stop(self->server);
        self->running = false;
        ESP_LOGI(TAG, "Web server stopped");
    }
}

// Constructor
WebServer_t *webserver_create(Camera_t *camera, HttpClient_t *http_client, SettingsManager_t *settings, LEDRing_t *led_ring, ThermalPrinter_t *printer)
{
    WebServer_t *server = malloc(sizeof(WebServer_t));
    if (!server)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for WebServer");
        return NULL;
    }

    server->server = NULL;
    server->camera = camera;
    server->http_client = http_client;
    server->settings = settings;
    server->led_ring = led_ring;
    server->printer = printer;
    server->running = false;
    server->start = webserver_start_impl;
    server->stop = webserver_stop_impl;

    g_camera = camera; // Set global references
    g_http_client = http_client;
    g_settings = settings;
    g_led_ring = led_ring;
    g_printer = printer;

    return server;
}

// Destructor
void webserver_destroy(WebServer_t *server)
{
    if (server)
    {
        if (server->running)
        {
            server->stop(server);
        }
        free(server);
    }
}
