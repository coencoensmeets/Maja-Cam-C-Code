#ifndef CAMERA_H
#define CAMERA_H

#include "esp_camera.h"
#include "led.h"
#include <stdbool.h>

// Camera "Class" - Handles OV2640 camera operations
typedef struct Camera_t {
    camera_config_t config;
    bool initialized;
    LED_t* status_led;
    
    // Methods
    esp_err_t (*init)(struct Camera_t* self);
    camera_fb_t* (*capture)(struct Camera_t* self);
    void (*return_frame)(struct Camera_t* self, camera_fb_t* fb);
    void (*print_info)(struct Camera_t* self, camera_fb_t* fb);
} Camera_t;

// Constructor
Camera_t* camera_create(LED_t* led);

// Destructor
void camera_destroy(Camera_t* camera);

#endif // CAMERA_H
