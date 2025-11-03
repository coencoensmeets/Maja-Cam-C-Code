#include "wifi_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_system.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "WIFI";

// WiFi event handler implementation
static void wifi_event_handler_impl(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data)
{
    WiFi_t *wifi = (WiFi_t *)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "Disconnected from WiFi, retrying...");
        wifi->connected = false;
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi->connected = true;
        wifi->status_led->blink(wifi->status_led, 3);
    }
}

static esp_err_t wifi_init_impl(WiFi_t *self)
{
    ESP_LOGI(TAG, "Initializing WiFi...");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_event_handler_impl, self));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               wifi_event_handler_impl, self));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, self->ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, self->password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi connecting to %s...", self->ssid);
    return ESP_OK;
}

// Wait for WiFi connection with timeout
static bool wifi_wait_for_connection_impl(WiFi_t *self, uint32_t timeout_ms)
{
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    uint32_t elapsed = 0;
    uint32_t check_interval = 100; // Check every 100ms

    while (elapsed < timeout_ms)
    {
        if (self->connected)
        {
            ESP_LOGI(TAG, "WiFi connected successfully!");
            return true;
        }
        vTaskDelay(check_interval / portTICK_PERIOD_MS);
        elapsed += check_interval;
    }

    ESP_LOGW(TAG, "WiFi connection timeout after %lu ms", timeout_ms);
    return false;
}

// Wait for WiFi connection with infinite retries
static void wifi_wait_for_connection_retry_impl(WiFi_t *self)
{
    ESP_LOGI(TAG, "Waiting for WiFi connection (will retry indefinitely)...");
    ESP_LOGI(TAG, "Board will restart if no connection within 10 seconds");
    uint32_t seconds_elapsed = 0;
    uint32_t check_interval = 1000;      // Check every 1 second
    const uint32_t restart_timeout = 10; // Restart after 10 seconds

    while (!self->connected)
    {
        // Wait and check
        vTaskDelay(check_interval / portTICK_PERIOD_MS);
        seconds_elapsed++;

        // Check if we've exceeded the restart timeout
        if (seconds_elapsed >= restart_timeout)
        {
            ESP_LOGE(TAG, "WiFi connection failed after %lu seconds!", seconds_elapsed);
            ESP_LOGE(TAG, "Restarting board in 3 seconds...");
            self->status_led->blink(self->status_led, 10); // Rapid blink before restart
            vTaskDelay(3000 / portTICK_PERIOD_MS);
            esp_restart(); // Restart the board
        }

        // Log status every 10 seconds
        if (seconds_elapsed % 10 == 0)
        {
            ESP_LOGW(TAG, "Still waiting for WiFi connection... (%lu seconds elapsed)",
                     seconds_elapsed);
            ESP_LOGI(TAG, "SSID: %s", self->ssid);
            ESP_LOGW(TAG, "Will restart in %lu seconds if not connected",
                     restart_timeout - seconds_elapsed);
            self->status_led->blink(self->status_led, 2);
        }
    }

    ESP_LOGI(TAG, "WiFi connected successfully after %lu seconds!",
             seconds_elapsed);
}

// Get IP address as string
static char *wifi_get_ip_address_impl(WiFi_t *self)
{
    static char ip_str[16];
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    if (netif == NULL || !self->connected)
    {
        strcpy(ip_str, "Not Connected");
        return ip_str;
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
    {
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
    }
    else
    {
        strcpy(ip_str, "Error");
    }

    return ip_str;
}

// Constructor
WiFi_t *wifi_create(const char *ssid, const char *password, LED_t *led)
{
    WiFi_t *wifi = malloc(sizeof(WiFi_t));
    if (!wifi)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for WiFi");
        return NULL;
    }

    strncpy(wifi->ssid, ssid, sizeof(wifi->ssid) - 1);
    strncpy(wifi->password, password, sizeof(wifi->password) - 1);
    wifi->connected = false;
    wifi->status_led = led;
    wifi->init = wifi_init_impl;
    wifi->wait_for_connection = wifi_wait_for_connection_impl;
    wifi->wait_for_connection_retry = wifi_wait_for_connection_retry_impl;
    wifi->get_ip_address = wifi_get_ip_address_impl;
    wifi->event_handler = wifi_event_handler_impl;

    return wifi;
}

// Destructor
void wifi_destroy(WiFi_t *wifi)
{
    if (wifi)
    {
        esp_wifi_stop();
        esp_wifi_deinit();
        free(wifi);
    }
}
