# API Reference

## LED Class API

### Constructor

```c
LED_t* led_create(gpio_num_t pin);
```

Creates a new LED object on specified GPIO pin.

### Methods

```c
led->init(led);                // Initialize GPIO pin
led->blink(led, times);        // Blink LED N times (100ms on/off)
led->on(led);                  // Turn LED on
led->off(led);                 // Turn LED off
```

### Destructor

```c
void led_destroy(LED_t* led);
```

---

## Camera Class API

### Constructor

```c
Camera_t* camera_create(LED_t* led);
```

Creates camera object with status LED reference.

### Methods

```c
esp_err_t err = camera->init(camera);           // Initialize OV2640 camera
camera_fb_t* fb = camera->capture(camera);      // Capture JPEG image
camera->print_info(camera, fb);                 // Print image info to log
camera->return_frame(camera, fb);               // Return frame buffer
```

### Properties

```c
camera->initialized   // true if camera init successful
camera->config        // camera_config_t structure
```

### Destructor

```c
void camera_destroy(Camera_t* camera);
```

---

## WiFi Class API

### Constructor

```c
WiFi_t* wifi_create(const char* ssid, const char* password, LED_t* led);
```

Creates WiFi manager with credentials and status LED.

### Methods

```c
esp_err_t err = wifi->init(wifi);   // Initialize and connect to WiFi
```

### Properties

```c
wifi->connected     // true when connected to WiFi
wifi->ssid[32]      // WiFi SSID
wifi->password[64]  // WiFi password
```

### Destructor

```c
void wifi_destroy(WiFi_t* wifi);
```

---

## WebServer Class API

### Constructor

```c
WebServer_t* server = webserver_create(Camera_t* camera);
```

Creates HTTP server with camera reference.

### Methods

```c
esp_err_t err = server->start(server);  // Start HTTP server
server->stop(server);                    // Stop HTTP server
```

### Endpoints

- `GET /` - HTML camera interface
- `GET /capture` - JPEG image capture

### Properties

```c
server->running     // true if server is running
server->server      // httpd_handle_t
```

### Destructor

```c
void webserver_destroy(WebServer_t* server);
```

---

## Complete Example

```c
#include "led.h"
#include "camera.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "secrets.h"

void app_main(void) {
    // Create objects
    LED_t* led = led_create(GPIO_NUM_2);
    Camera_t* camera = camera_create(led);
    WiFi_t* wifi = wifi_create(WIFI_SSID, WIFI_PASS, led);
    WebServer_t* server = webserver_create(camera);

    // Initialize
    led->init(led);
    led->blink(led, 3);

    if (camera->init(camera) != ESP_OK) {
        ESP_LOGE("MAIN", "Camera failed!");
        return;
    }

    wifi->init(wifi);
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    if (server->start(server) == ESP_OK) {
        ESP_LOGI("MAIN", "Server running!");
    }

    // Main loop
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // Cleanup (optional)
    webserver_destroy(server);
    wifi_destroy(wifi);
    camera_destroy(camera);
    led_destroy(led);
}
```
