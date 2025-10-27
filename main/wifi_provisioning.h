#ifndef WIFI_PROVISIONING_H
#define WIFI_PROVISIONING_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "led.h"
#include <stdbool.h>

// WiFi credentials storage
typedef struct {
    char ssid[32];
    char password[64];
    bool is_provisioned;
} wifi_credentials_t;

// WiFi Provisioning "Class"
typedef struct WiFiProvisioning_t {
    LED_t* status_led;
    httpd_handle_t server;
    bool provisioning_complete;
    wifi_credentials_t credentials;
    
    // Methods
    esp_err_t (*init)(struct WiFiProvisioning_t* self);
    esp_err_t (*start_ap)(struct WiFiProvisioning_t* self);
    esp_err_t (*start_portal)(struct WiFiProvisioning_t* self);
    bool (*wait_for_credentials)(struct WiFiProvisioning_t* self, uint32_t timeout_ms);
    wifi_credentials_t* (*get_credentials)(struct WiFiProvisioning_t* self);
    void (*stop)(struct WiFiProvisioning_t* self);
} WiFiProvisioning_t;

// Constructor
WiFiProvisioning_t* wifi_provisioning_create(LED_t* led);

// Destructor
void wifi_provisioning_destroy(WiFiProvisioning_t* prov);

// Helper functions to load/save credentials from NVS (Non-Volatile Storage)
esp_err_t wifi_credentials_load(wifi_credentials_t* creds);
esp_err_t wifi_credentials_save(const wifi_credentials_t* creds);
esp_err_t wifi_credentials_clear(void);
bool wifi_credentials_exist(void);

#endif // WIFI_PROVISIONING_H
