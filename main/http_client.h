#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "esp_err.h"
#include "camera.h"
#include "settings_manager.h"

// HTTP Client "Class" for uploading images to Flask server
typedef struct HttpClient_t
{
    char server_url[256];
    Camera_t *camera;
    SettingsManager_t *settings;
    bool running;

    // Methods
    esp_err_t (*init)(struct HttpClient_t *self);
    esp_err_t (*upload_image)(struct HttpClient_t *self, camera_fb_t *fb);
    esp_err_t (*capture_and_upload)(struct HttpClient_t *self);
    esp_err_t (*post_json)(struct HttpClient_t *self, const char *endpoint, const char *json_data);
    void (*start_auto_upload_task)(struct HttpClient_t *self);
    void (*stop_auto_upload_task)(struct HttpClient_t *self);
} HttpClient_t;

// Constructor
HttpClient_t *http_client_create(Camera_t *camera, SettingsManager_t *settings);

// Destructor
void http_client_destroy(HttpClient_t *client);

#endif // HTTP_CLIENT_H
