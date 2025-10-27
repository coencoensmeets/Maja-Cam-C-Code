# Flask Server Updates - Quick Guide

## New Features Added ✨

### 1. "Take Picture" Button

- Click to trigger ESP32 to capture an image immediately
- Image automatically uploads and appears in gallery
- Located in the header of the main page

### 2. Settings Page

- Configure ESP32 IP address
- View and update ESP32 camera settings:
  - Camera quality, frame size
  - Image flips (horizontal/vertical)
  - Brightness, contrast, saturation
  - Upload URL and interval
  - LED settings
- Access via "⚙️ Settings" button in header

## Setup Instructions

### 1. Install New Dependency

```bash
cd flask_server
pip install requests
```

Or reinstall all requirements:

```bash
pip install -r requirements.txt
```

### 2. Configure ESP32 IP Address

Edit `flask_server/.env` and add your ESP32's IP:

```env
ESP32_IP=192.168.178.100
```

Replace with your actual ESP32 IP address (find it in the ESP32 serial monitor).

### 3. ESP32 Code Updates Needed

The ESP32 needs new API endpoints to support these features. Add to `web_server.c`:

**Required endpoints:**

- `GET /trigger-capture` - Triggers immediate photo capture
- `GET /api/settings` - Returns current settings as JSON
- `POST /api/settings` - Updates settings from JSON

See below for implementation details.

## Usage

### Take Picture

1. Open Flask website: `http://localhost:5000`
2. Click "📸 Take Picture" button
3. ESP32 captures and uploads image
4. Image appears in gallery after ~2 seconds

### Configure Settings

1. Click "⚙️ Settings" in header
2. Update ESP32 IP if needed
3. Load current ESP32 settings
4. Modify camera/upload settings
5. Click "Save ESP32 Settings"
6. Restart ESP32 for changes to take effect

## ESP32 API Endpoints Needed

Add these handlers to your `web_server.c`:

### 1. Trigger Capture Endpoint

```c
static esp_err_t trigger_capture_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Trigger capture request received");

    // Trigger the http_client to capture and upload
    if (g_http_client && g_http_client->capture_and_upload) {
        esp_err_t ret = g_http_client->capture_and_upload(g_http_client);

        if (ret == ESP_OK) {
            const char *resp = "{\"success\":true,\"message\":\"Capture triggered\"}";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
    }

    const char *resp = "{\"success\":false,\"error\":\"Capture failed\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_FAIL;
}
```

### 2. Get Settings Endpoint

```c
static esp_err_t get_settings_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Get settings request received");

    // Export settings as JSON
    char *json_str = NULL;
    if (g_settings && g_settings->export_settings_json) {
        g_settings->export_settings_json(g_settings, &json_str);

        if (json_str) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
            free(json_str);
            return ESP_OK;
        }
    }

    httpd_resp_send_500(req);
    return ESP_FAIL;
}
```

### 3. Update Settings Endpoint

```c
static esp_err_t update_settings_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Update settings request received");

    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    buf[ret] = '\0';

    // Import settings from JSON
    if (g_settings && g_settings->import_settings_json) {
        if (g_settings->import_settings_json(g_settings, buf) == ESP_OK) {
            const char *resp = "{\"success\":true,\"message\":\"Settings updated\"}";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
    }

    const char *resp = "{\"success\":false,\"error\":\"Failed to update settings\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_FAIL;
}
```

### 4. Register Routes

In `webserver_start_impl()`, add:

```c
httpd_uri_t trigger_capture_uri = {
    .uri = "/trigger-capture",
    .method = HTTP_GET,
    .handler = trigger_capture_handler,
    .user_ctx = NULL
};
httpd_register_uri_handler(self->server, &trigger_capture_uri);

httpd_uri_t get_settings_uri = {
    .uri = "/api/settings",
    .method = HTTP_GET,
    .handler = get_settings_handler,
    .user_ctx = NULL
};
httpd_register_uri_handler(self->server, &get_settings_uri);

httpd_uri_t update_settings_uri = {
    .uri = "/api/settings",
    .method = HTTP_POST,
    .handler = update_settings_handler,
    .user_ctx = NULL
};
httpd_register_uri_handler(self->server, &update_settings_uri);
```

### 5. Add Global References

At the top of `web_server.c`:

```c
static Camera_t *g_camera = NULL;
static HttpClient_t *g_http_client = NULL;  // Add this
static SettingsManager_t *g_settings = NULL;  // Add this
```

Update the constructor to accept these:

```c
WebServer_t *webserver_create(Camera_t *camera, HttpClient_t *http_client, SettingsManager_t *settings)
{
    // ... existing code ...

    g_camera = camera;
    g_http_client = http_client;
    g_settings = settings;

    // ... rest of code ...
}
```

## Testing

### 1. Start Flask Server

```bash
cd flask_server
python run.py
```

### 2. Flash ESP32

```bash
idf.py build flash monitor
```

### 3. Test Features

1. Open `http://localhost:5000`
2. Click "📸 Take Picture" - should trigger ESP32 capture
3. Click "⚙️ Settings"
4. Update ESP32 IP if needed
5. Load and modify settings
6. Save settings

## Troubleshooting

### "Take Picture" Not Working

- ✅ Check ESP32 IP in settings page
- ✅ Verify ESP32 is online
- ✅ Check ESP32 logs for `/trigger-capture` request
- ✅ Ensure Flask server can reach ESP32

### Settings Page Shows Loading

- ✅ ESP32 IP must be correct
- ✅ ESP32 must be running and accessible
- ✅ Check browser console for errors (F12)
- ✅ Verify `/api/settings` endpoint exists on ESP32

### Changes Not Saving

- ✅ ESP32 must restart for most settings
- ✅ Check SPIFFS is working
- ✅ Verify settings.json is writable
- ✅ Check ESP32 serial logs for errors

## What's New in Files

**Modified:**

- `flask_server/app.py` - Added routes for trigger, settings, ESP32 IP
- `flask_server/templates/index.html` - Added Take Picture and Settings buttons
- `flask_server/static/js/main.js` - Added takePicture() function
- `flask_server/static/css/style.css` - Added settings page styles
- `flask_server/requirements.txt` - Added requests library
- `flask_server/.env` - Added ESP32_IP configuration

**Created:**

- `flask_server/templates/settings.html` - Full settings page

**ESP32 Updates Needed:**

- `main/web_server.c` - Add 3 new API endpoints
- `main/web_server.h` - Update constructor signature

---

**Enjoy your enhanced Flask camera interface!** 📸⚙️
