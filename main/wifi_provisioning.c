#include "wifi_provisioning.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_http_server.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "WIFI_PROV";
static const char *NVS_NAMESPACE = "wifi_creds";

// Access Point configuration
#define AP_SSID "ESP32-Camera-Setup"
#define AP_PASSWORD "" // Open network
#define AP_MAX_CONNECTIONS 4

// HTML page for WiFi configuration
static const char *provisioning_html =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<title>ESP32 Camera Setup</title>"
    "<style>"
    "body { font-family: Arial, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); "
    "       margin: 0; padding: 20px; min-height: 100vh; display: flex; justify-content: center; align-items: center; }"
    ".container { background: white; padding: 40px; border-radius: 10px; box-shadow: 0 10px 40px rgba(0,0,0,0.2); "
    "            max-width: 400px; width: 100%; }"
    "h1 { color: #333; margin-top: 0; text-align: center; font-size: 24px; }"
    "label { display: block; margin-top: 20px; color: #555; font-weight: bold; }"
    "input { width: 100%; padding: 12px; margin-top: 8px; border: 2px solid #ddd; border-radius: 5px; "
    "        box-sizing: border-box; font-size: 16px; transition: border-color 0.3s; }"
    "input:focus { border-color: #667eea; outline: none; }"
    "button { width: 100%; padding: 14px; margin-top: 30px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); "
    "         color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; font-weight: bold; "
    "         transition: transform 0.2s; }"
    "button:hover { transform: translateY(-2px); }"
    "button:active { transform: translateY(0); }"
    ".info { text-align: center; color: #666; margin-top: 20px; font-size: 14px; }"
    ".icon { text-align: center; font-size: 48px; margin-bottom: 20px; }"
    "</style>"
    "</head>"
    "<body>"
    "<div class='container'>"
    "<div class='icon'>📷</div>"
    "<h1>ESP32 Camera Setup</h1>"
    "<p class='info'>Enter your WiFi credentials to connect the camera to your network</p>"
    "<form action='/save' method='POST'>"
    "<label for='ssid'>WiFi Network (SSID)</label>"
    "<input type='text' id='ssid' name='ssid' required placeholder='Enter WiFi name'>"
    "<label for='password'>Password</label>"
    "<input type='password' id='password' name='password' required placeholder='Enter WiFi password'>"
    "<button type='submit'>Connect</button>"
    "</form>"
    "</div>"
    "</body>"
    "</html>";

static const char *success_html =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<title>Success</title>"
    "<style>"
    "body { font-family: Arial, sans-serif; background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%); "
    "       margin: 0; padding: 20px; min-height: 100vh; display: flex; justify-content: center; align-items: center; }"
    ".container { background: white; padding: 40px; border-radius: 10px; box-shadow: 0 10px 40px rgba(0,0,0,0.2); "
    "            max-width: 400px; width: 100%; text-align: center; }"
    "h1 { color: #11998e; margin-top: 0; font-size: 24px; }"
    ".icon { font-size: 64px; margin-bottom: 20px; }"
    "p { color: #555; line-height: 1.6; }"
    "</style>"
    "</head>"
    "<body>"
    "<div class='container'>"
    "<div class='icon'>✓</div>"
    "<h1>Configuration Saved!</h1>"
    "<p>Your ESP32 Camera is connecting to the WiFi network.</p>"
    "<p>This setup network will close in a few seconds.</p>"
    "<p>Please reconnect to your main WiFi and access the camera.</p>"
    "</div>"
    "</body>"
    "</html>";

// NVS Helper Functions
esp_err_t wifi_credentials_save(const wifi_credentials_t *creds)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(nvs_handle, "ssid", creds->ssid);
    if (err != ESP_OK)
        goto exit;

    err = nvs_set_str(nvs_handle, "password", creds->password);
    if (err != ESP_OK)
        goto exit;

    err = nvs_set_u8(nvs_handle, "provisioned", creds->is_provisioned ? 1 : 0);
    if (err != ESP_OK)
        goto exit;

    err = nvs_commit(nvs_handle);
    ESP_LOGI(TAG, "WiFi credentials saved to NVS");

exit:
    nvs_close(nvs_handle);
    return err;
}

esp_err_t wifi_credentials_load(wifi_credentials_t *creds)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    size_t ssid_len = sizeof(creds->ssid);
    err = nvs_get_str(nvs_handle, "ssid", creds->ssid, &ssid_len);
    if (err != ESP_OK)
        goto exit;

    size_t pass_len = sizeof(creds->password);
    err = nvs_get_str(nvs_handle, "password", creds->password, &pass_len);
    if (err != ESP_OK)
        goto exit;

    uint8_t provisioned;
    err = nvs_get_u8(nvs_handle, "provisioned", &provisioned);
    if (err != ESP_OK)
        goto exit;

    creds->is_provisioned = (provisioned == 1);
    ESP_LOGI(TAG, "WiFi credentials loaded from NVS");

exit:
    nvs_close(nvs_handle);
    return err;
}

esp_err_t wifi_credentials_clear(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
        return err;

    err = nvs_erase_all(nvs_handle);
    if (err == ESP_OK)
    {
        nvs_commit(nvs_handle);
        ESP_LOGI(TAG, "WiFi credentials cleared");
    }

    nvs_close(nvs_handle);
    return err;
}

bool wifi_credentials_exist(void)
{
    wifi_credentials_t creds;
    esp_err_t err = wifi_credentials_load(&creds);
    return (err == ESP_OK && creds.is_provisioned);
}

// HTTP handlers
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, provisioning_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t save_handler(httpd_req_t *req)
{
    WiFiProvisioning_t *prov = (WiFiProvisioning_t *)req->user_ctx;
    char buf[256];
    int ret, remaining = req->content_len;

    // Read POST data
    if (remaining >= sizeof(buf))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Parse form data (ssid=xxx&password=yyy)
    char ssid[32] = {0};
    char password[64] = {0};

    char *ssid_ptr = strstr(buf, "ssid=");
    char *pass_ptr = strstr(buf, "password=");

    if (ssid_ptr && pass_ptr)
    {
        ssid_ptr += 5; // Skip "ssid="
        char *ssid_end = strchr(ssid_ptr, '&');
        if (ssid_end)
        {
            int ssid_len = ssid_end - ssid_ptr;
            strncpy(ssid, ssid_ptr, ssid_len < 32 ? ssid_len : 31);
        }

        pass_ptr += 9; // Skip "password="
        strncpy(password, pass_ptr, 63);
    }

    // URL decode (basic - handle %20 for spaces)
    for (int i = 0; ssid[i]; i++)
    {
        if (ssid[i] == '+')
            ssid[i] = ' ';
    }
    for (int i = 0; password[i]; i++)
    {
        if (password[i] == '+')
            password[i] = ' ';
    }

    ESP_LOGI(TAG, "Received credentials - SSID: %s", ssid);

    // Save credentials
    strncpy(prov->credentials.ssid, ssid, sizeof(prov->credentials.ssid) - 1);
    strncpy(prov->credentials.password, password, sizeof(prov->credentials.password) - 1);
    prov->credentials.is_provisioned = true;

    wifi_credentials_save(&prov->credentials);
    prov->provisioning_complete = true;

    // Send success page
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, success_html, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

// Captive portal DNS handler (redirects all requests to setup page)
static esp_err_t captive_handler(httpd_req_t *req)
{
    const char *redirect =
        "HTTP/1.1 302 Found\r\n"
        "Location: http://192.168.4.1/\r\n"
        "Connection: close\r\n"
        "\r\n";
    httpd_resp_send(req, redirect, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Methods implementation
static esp_err_t wifi_provisioning_init_impl(WiFiProvisioning_t *self)
{
    ESP_LOGI(TAG, "Initializing WiFi provisioning...");

    // Initialize NVS if not already done
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Try to load existing credentials
    if (wifi_credentials_load(&self->credentials) == ESP_OK && self->credentials.is_provisioned)
    {
        ESP_LOGI(TAG, "Found saved credentials for: %s", self->credentials.ssid);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "No saved credentials found");
    return ESP_OK;
}

static esp_err_t wifi_provisioning_start_ap_impl(WiFiProvisioning_t *self)
{
    ESP_LOGI(TAG, "Starting Access Point: %s", AP_SSID);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = 1,
            .password = AP_PASSWORD,
            .max_connection = AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_OPEN},
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Access Point started successfully!");
    ESP_LOGI(TAG, "Connect to WiFi: %s", AP_SSID);
    ESP_LOGI(TAG, "Then open: http://192.168.4.1");

    // Blink LED to indicate AP mode
    if (self->status_led)
    {
        self->status_led->blink(self->status_led, 2);
    }

    return ESP_OK;
}

static esp_err_t wifi_provisioning_start_portal_impl(WiFiProvisioning_t *self)
{
    ESP_LOGI(TAG, "Starting provisioning web portal...");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 8;

    if (httpd_start(&self->server, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    // Root handler
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = self};
    httpd_register_uri_handler(self->server, &root_uri);

    // Save handler
    httpd_uri_t save_uri = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_handler,
        .user_ctx = self};
    httpd_register_uri_handler(self->server, &save_uri);

    ESP_LOGI(TAG, "Provisioning portal started!");
    return ESP_OK;
}

static bool wifi_provisioning_wait_impl(WiFiProvisioning_t *self, uint32_t timeout_ms)
{
    ESP_LOGI(TAG, "Waiting for credentials (timeout: %lu ms)...", timeout_ms);
    uint32_t elapsed = 0;
    uint32_t check_interval = 500;

    while (elapsed < timeout_ms)
    {
        if (self->provisioning_complete)
        {
            ESP_LOGI(TAG, "Provisioning complete!");
            return true;
        }
        vTaskDelay(check_interval / portTICK_PERIOD_MS);
        elapsed += check_interval;
    }

    ESP_LOGW(TAG, "Provisioning timeout");
    return false;
}

static wifi_credentials_t *wifi_provisioning_get_credentials_impl(WiFiProvisioning_t *self)
{
    return &self->credentials;
}

static void wifi_provisioning_stop_impl(WiFiProvisioning_t *self)
{
    if (self->server)
    {
        httpd_stop(self->server);
        self->server = NULL;
        ESP_LOGI(TAG, "Provisioning portal stopped");
    }

    esp_wifi_stop();
    ESP_LOGI(TAG, "Access Point stopped");
}

// Constructor
WiFiProvisioning_t *wifi_provisioning_create(LED_t *led)
{
    WiFiProvisioning_t *prov = malloc(sizeof(WiFiProvisioning_t));
    if (!prov)
    {
        ESP_LOGE(TAG, "Failed to allocate memory");
        return NULL;
    }

    memset(prov, 0, sizeof(WiFiProvisioning_t));
    prov->status_led = led;
    prov->provisioning_complete = false;
    prov->init = wifi_provisioning_init_impl;
    prov->start_ap = wifi_provisioning_start_ap_impl;
    prov->start_portal = wifi_provisioning_start_portal_impl;
    prov->wait_for_credentials = wifi_provisioning_wait_impl;
    prov->get_credentials = wifi_provisioning_get_credentials_impl;
    prov->stop = wifi_provisioning_stop_impl;

    return prov;
}

// Destructor
void wifi_provisioning_destroy(WiFiProvisioning_t *prov)
{
    if (prov)
    {
        if (prov->server)
        {
            httpd_stop(prov->server);
        }
        free(prov);
    }
}
