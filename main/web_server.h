#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_http_server.h"
#include "camera.h"
#include "http_client.h"
#include "settings_manager.h"
#include "led_ring.h"
#include <stdbool.h>

// WebServer "Class" - Handles HTTP server
typedef struct WebServer_t
{
    httpd_handle_t server;
    Camera_t *camera;
    HttpClient_t *http_client;
    SettingsManager_t *settings;
    LEDRing_t *led_ring;
    bool running;

    // Methods
    esp_err_t (*start)(struct WebServer_t *self);
    void (*stop)(struct WebServer_t *self);
} WebServer_t;

// Constructor
WebServer_t *webserver_create(Camera_t *camera, HttpClient_t *http_client, SettingsManager_t *settings, LEDRing_t *led_ring);

// Destructor
void webserver_destroy(WebServer_t *server);

#endif // WEB_SERVER_H
