# ESP32 Flask Server Integration - Setup Guide

## Overview

Your ESP32 camera now uploads images to your Flask server instead of (or in addition to) hosting them locally. The system includes:

- **Automatic uploads** to your Flask server at configurable intervals
- **Configurable server URL** via settings.json
- **Optional local web server** for live viewing
- **Robust error handling** and logging

## Changes Made

### New Files

1. **`main/http_client.h`** - HTTP client header
2. **`main/http_client.c`** - HTTP client implementation with auto-upload task

### Modified Files

1. **`main/settings_manager.h`** - Added server upload settings
2. **`main/settings_manager.c`** - Implemented server settings handling
3. **`main/main.c`** - Integrated HTTP client, kept optional web server
4. **`main/CMakeLists.txt`** - Added http_client and esp_http_client
5. **`data/settings.json.example`** - Added server configuration section

## Configuration

### 1. Update settings.json

Edit `data/settings.json` (or create from example):

```json
{
  "version": 1,
  "camera": {
    "quality": 12,
    "framesize": 10,
    "flip_horizontal": false,
    "flip_vertical": false,
    "brightness": 0,
    "contrast": 0,
    "saturation": 0
  },
  "system": {
    "device_name": "ESP32-Camera",
    "led_enabled": true,
    "http_port": 80
  },
  "server": {
    "upload_url": "http://192.168.1.100:5000/api/capture",
    "upload_enabled": true,
    "upload_interval_seconds": 30
  }
}
```

### 2. Server Settings Explained

- **`upload_url`**: Full URL to your Flask server's `/api/capture` endpoint
  - Local network: `http://192.168.1.100:5000/api/capture`
  - PythonAnywhere: `https://yourusername.pythonanywhere.com/api/capture`
- **`upload_enabled`**: Enable/disable automatic uploads

  - `true`: Automatically upload at specified intervals
  - `false`: Disable auto-upload (can still capture manually)

- **`upload_interval_seconds`**: Time between uploads (minimum: 5 seconds)
  - Default: `30` (uploads every 30 seconds)
  - Adjust based on your needs (e.g., 60 for every minute, 300 for every 5 minutes)

## How It Works

### System Flow

```
ESP32 Camera
    │
    ├──► Captures image every N seconds
    │
    ├──► Uploads to Flask Server (if enabled)
    │       └─► POST /api/capture
    │               └─► Image stored in Flask uploads/
    │
    └──► Still serves local web page (optional)
            └─► http://esp32-ip-address/
```

### Auto Upload Task

When enabled, the ESP32:

1. Captures an image using the camera
2. Packages it as multipart/form-data
3. POSTs to your Flask server's `/api/capture` endpoint
4. Waits for the configured interval
5. Repeats

### Features

✅ **Automatic uploads** at configurable intervals  
✅ **Configurable server URL** via settings  
✅ **Enable/disable** uploads without reflashing  
✅ **Robust error handling** with retry logic  
✅ **Status logging** for debugging  
✅ **Optional local server** for direct viewing  
✅ **Low memory footprint** with efficient buffering

## Building and Flashing

### 1. Build the Project

```bash
idf.py build
```

### 2. Flash to ESP32

```bash
idf.py flash
```

### 3. Monitor Output

```bash
idf.py monitor
```

You should see logs like:

```
I (12345) HTTP_CLIENT: HTTP Client initialized
I (12346) HTTP_CLIENT: Server URL: http://192.168.1.100:5000/api/capture
I (12347) HTTP_CLIENT: Upload enabled: Yes
I (12348) HTTP_CLIENT: Auto-upload task started (interval: 30 seconds)
```

## Testing

### 1. Start Your Flask Server

```bash
cd flask_server
python run.py
```

Server should be running at `http://127.0.0.1:5000` or your configured address.

### 2. Flash and Monitor ESP32

```bash
idf.py flash monitor
```

### 3. Watch the Logs

You should see:

```
I (xxxxx) HTTP_CLIENT: Starting scheduled capture and upload...
I (xxxxx) HTTP_CLIENT: Capturing image...
I (xxxxx) HTTP_CLIENT: Image captured: 15234 bytes
I (xxxxx) HTTP_CLIENT: Uploading image to server... Size: 15234 bytes
I (xxxxx) HTTP_CLIENT: HTTP POST Status = 200, content_length = 78
I (xxxxx) HTTP_CLIENT: ✓ Image uploaded successfully!
I (xxxxx) HTTP_CLIENT: ✓ Scheduled upload completed successfully
```

### 4. Check Flask Server

Open `http://localhost:5000` in your browser - you should see uploaded images in the gallery!

## Troubleshooting

### Upload Fails - Connection Refused

**Problem**: `HTTP POST request failed: ESP_ERR_HTTP_CONNECT`

**Solution**:

1. Verify Flask server is running
2. Check IP address in settings.json matches your server
3. Ensure ESP32 and server are on same network
4. Try `ping` from computer to ESP32 IP

### Upload Fails - 404 Not Found

**Problem**: `Server returned error status: 404`

**Solution**:

1. Verify URL includes `/api/capture` endpoint
2. Check Flask server logs for route registration
3. Ensure `app.py` has the capture route

### Settings Not Loading

**Problem**: ESP32 uses default URL

**Solution**:

1. Check SPIFFS partition exists: `idf.py partition-table`
2. Verify settings.json is in `data/` folder
3. Re-flash SPIFFS: See SPIFFS upload instructions
4. Check ESP32 logs for SPIFFS errors

### Images Too Large

**Problem**: Upload times out or fails

**Solution**:

1. Reduce camera quality in settings.json (higher number = lower quality)
2. Reduce framesize (e.g., from 10 to 8 for VGA)
3. Increase timeout in `http_client.c` (currently 10 seconds)

### Auto Upload Not Starting

**Problem**: No upload task logs

**Solution**:

1. Check `upload_enabled` is `true` in settings.json
2. Verify WiFi is connected
3. Check free heap: `esp_get_free_heap_size()`
4. Look for task creation errors in logs

## Configuration Examples

### High Frequency (Every 10 seconds)

```json
"server": {
  "upload_url": "http://192.168.1.100:5000/api/capture",
  "upload_enabled": true,
  "upload_interval_seconds": 10
}
```

### Low Frequency (Every 5 minutes)

```json
"server": {
  "upload_url": "http://192.168.1.100:5000/api/capture",
  "upload_enabled": true,
  "upload_interval_seconds": 300
}
```

### PythonAnywhere Server

```json
"server": {
  "upload_url": "https://yourusername.pythonanywhere.com/api/capture",
  "upload_enabled": true,
  "upload_interval_seconds": 30
}
```

### Disabled (Local Only)

```json
"server": {
  "upload_url": "http://192.168.1.100:5000/api/capture",
  "upload_enabled": false,
  "upload_interval_seconds": 30
}
```

## Advanced Usage

### Disable Local Web Server

If you only want uploads (no local web viewing), comment out in `main.c`:

```c
// Optional: Still create web server for local viewing (comment out if not needed)
// WebServer_t *server = webserver_create(camera);
// if (!server) { ... }
```

And in the initialization section:

```c
// Comment out the web server start section
```

This saves memory and resources.

### Change Upload Interval Dynamically

You can change settings without reflashing:

1. Edit `data/settings.json` on SPIFFS
2. Restart ESP32
3. New settings will be loaded

### Manual Upload Trigger

To trigger manual upload (for testing), you can add a button or call:

```c
http_client->capture_and_upload(http_client);
```

## API Reference

### HttpClient Methods

```c
// Initialize HTTP client
esp_err_t init(HttpClient_t* self);

// Upload a captured image
esp_err_t upload_image(HttpClient_t* self, camera_fb_t *fb);

// Capture and upload in one call
esp_err_t capture_and_upload(HttpClient_t* self);

// Start auto-upload background task
void start_auto_upload_task(HttpClient_t* self);

// Stop auto-upload background task
void stop_auto_upload_task(HttpClient_t* self);
```

### Settings Manager Methods (New)

```c
// Set Flask server upload URL
esp_err_t set_server_upload_url(SettingsManager_t* self, const char* url);

// Enable/disable uploads
esp_err_t set_server_upload_enabled(SettingsManager_t* self, bool enabled);

// Set upload interval (seconds, minimum 5)
esp_err_t set_server_upload_interval(SettingsManager_t* self, uint32_t interval);
```

## Performance Notes

- **Memory**: ~8KB per upload task
- **Bandwidth**: Depends on image size (typically 10-50 KB per image)
- **CPU**: Minimal impact, mostly waiting for network
- **Power**: Network transmission uses most power

## Security Considerations

⚠️ **Important**:

1. Use HTTPS for production (PythonAnywhere provides this)
2. Consider adding authentication to Flask endpoint
3. Don't expose ESP32 web server to internet
4. Use strong WiFi passwords
5. Keep firmware updated

## Next Steps

1. ✅ Configure `settings.json` with your Flask server URL
2. ✅ Build and flash ESP32
3. ✅ Start Flask server
4. ✅ Monitor uploads in real-time
5. ✅ Access gallery at Flask server URL
6. ✅ Deploy Flask server to PythonAnywhere (optional)

## Additional Resources

- **Flask Server Setup**: See `flask_server/README.md`
- **PythonAnywhere Deployment**: See `flask_server/DEPLOYMENT_GUIDE.md`
- **SPIFFS Guide**: See `docs/SPIFFS.md`
- **ESP32 Docs**: [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)

## Support

If you encounter issues:

1. Check ESP32 serial monitor logs (`idf.py monitor`)
2. Check Flask server logs (terminal output)
3. Verify network connectivity
4. Review configuration settings
5. Check free heap memory

---

**Congratulations!** Your ESP32 now uploads images to your Flask server automatically! 📸🚀
