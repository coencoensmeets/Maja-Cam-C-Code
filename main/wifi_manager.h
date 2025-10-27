#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_wifi.h"
#include "esp_event.h"
#include "led.h"
#include <stdbool.h>

// WiFi "Class" - Handles WiFi connection
typedef struct WiFi_t
{
    char ssid[32];
    char password[64];
    bool connected;
    LED_t *status_led;

    // Methods
    esp_err_t (*init)(struct WiFi_t *self);
    bool (*wait_for_connection)(struct WiFi_t *self, uint32_t timeout_ms);
    void (*wait_for_connection_retry)(struct WiFi_t *self);
    char *(*get_ip_address)(struct WiFi_t *self);
    void (*event_handler)(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);
} WiFi_t;

// Constructor
WiFi_t *wifi_create(const char *ssid, const char *password, LED_t *led);

// Destructor
void wifi_destroy(WiFi_t *wifi);

#endif // WIFI_MANAGER_H
