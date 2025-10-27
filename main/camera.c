#include "camera.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "CAMERA";

// Camera method implementations
static esp_err_t camera_init_impl(Camera_t *self)
{
    ESP_LOGI(TAG, "Initializing camera...");

    esp_err_t err = esp_camera_init(&self->config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        self->status_led->blink(self->status_led, 10);
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL)
    {
        ESP_LOGE(TAG, "Failed to get camera sensor");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Camera sensor detected! PID: 0x%02X", s->id.PID);

    // Configure sensor settings
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_gain_ctrl(s, 1);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);

    self->initialized = true;
    ESP_LOGI(TAG, "Camera initialized successfully");
    self->status_led->blink(self->status_led, 3);

    return ESP_OK;
}

static camera_fb_t *camera_capture_impl(Camera_t *self)
{
    if (!self->initialized)
    {
        ESP_LOGE(TAG, "Camera not initialized");
        return NULL;
    }

    // Discard the first frame to ensure we get a fresh capture
    camera_fb_t *fb_old = esp_camera_fb_get();
    if (fb_old)
    {
        esp_camera_fb_return(fb_old);
    }

    // Now capture a fresh frame
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb)
    {
        self->status_led->blink(self->status_led, 1);
    }
    else
    {
        ESP_LOGE(TAG, "Camera capture failed");
        self->status_led->blink(self->status_led, 5);
    }
    return fb;
}

static void camera_return_frame_impl(Camera_t *self, camera_fb_t *fb)
{
    if (fb)
    {
        esp_camera_fb_return(fb);
    }
}

static void camera_print_info_impl(Camera_t *self, camera_fb_t *fb)
{
    if (!fb)
        return;

    ESP_LOGI(TAG, "========== IMAGE INFO ==========");
    ESP_LOGI(TAG, "Size: %zu bytes", fb->len);
    ESP_LOGI(TAG, "Resolution: %dx%d", fb->width, fb->height);
    ESP_LOGI(TAG, "Format: %d", fb->format);
    ESP_LOGI(TAG, "================================");
}

// Constructor
Camera_t *camera_create(LED_t *led)
{
    Camera_t *cam = malloc(sizeof(Camera_t));
    if (!cam)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for Camera");
        return NULL;
    }

    // Pin configuration
    cam->config.pin_pwdn = -1;
    cam->config.pin_reset = -1;
    cam->config.pin_xclk = 15;
    cam->config.pin_sccb_sda = 4;
    cam->config.pin_sccb_scl = 5;
    cam->config.pin_d7 = 16;
    cam->config.pin_d6 = 17;
    cam->config.pin_d5 = 18;
    cam->config.pin_d4 = 12;
    cam->config.pin_d3 = 10;
    cam->config.pin_d2 = 8;
    cam->config.pin_d1 = 9;
    cam->config.pin_d0 = 11;
    cam->config.pin_vsync = 6;
    cam->config.pin_href = 7;
    cam->config.pin_pclk = 13;

    // Camera settings
    cam->config.xclk_freq_hz = 20000000;
    cam->config.ledc_timer = LEDC_TIMER_0;
    cam->config.ledc_channel = LEDC_CHANNEL_0;
    cam->config.pixel_format = PIXFORMAT_JPEG;
    cam->config.frame_size = FRAMESIZE_VGA;
    cam->config.jpeg_quality = 12;
    cam->config.fb_count = 2;
    cam->config.fb_location = CAMERA_FB_IN_PSRAM;
    cam->config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    cam->initialized = false;
    cam->status_led = led;

    // Bind methods
    cam->init = camera_init_impl;
    cam->capture = camera_capture_impl;
    cam->return_frame = camera_return_frame_impl;
    cam->print_info = camera_print_info_impl;

    return cam;
}

// Destructor
void camera_destroy(Camera_t *camera)
{
    if (camera)
    {
        if (camera->initialized)
        {
            esp_camera_deinit();
        }
        free(camera);
    }
}
