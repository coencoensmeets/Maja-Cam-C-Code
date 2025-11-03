# Camera Rotation Integration Guide

## Overview

Camera rotation has been fully integrated into the Poem Camera system, allowing users to rotate the camera image in 90-degree increments through the web interface, settings files, and remote commands.

## Features Added

### 1. **Settings Manager** (`settings_manager.h/c`)

- Added `camera_rotation` field to settings structure (0, 90, 180, 270)
- Added `set_camera_rotation()` method with validation
- Rotation is saved/loaded from `settings.json` in SPIFFS
- Default value: 0° (no rotation)

### 2. **Camera Module** (`camera.h/c`)

- Three new methods:
  - `camera->set_rotation(camera, degrees)` - Set rotation (0, 90, 180, 270)
  - `camera->set_hmirror(camera, enable)` - Horizontal flip
  - `camera->set_vflip(camera, enable)` - Vertical flip
- Rotation applied at hardware level (sensor) for best performance
- Automatically rounds to nearest 90°

### 3. **Main Application** (`main.c`)

- Rotation applied from settings on camera initialization
- Logs current rotation setting on boot

### 4. **Remote Control** (`remote_control.c`)

- Accepts rotation commands from Flask server
- Format: `{"camera": {"rotation": 90}}`
- Validates values (0, 90, 180, 270 only)
- Settings auto-saved on change

### 5. **Flask Web Interface** (`templates/settings.html`)

- Dropdown selector for rotation:
  - 0° (Normal)
  - 90° (Clockwise)
  - 180° (Upside Down)
  - 270° (Counter-Clockwise)
- Settings queued and sent to ESP32 on next poll
- Current rotation displayed on page load

### 6. **Flask Backend** (`app.py`)

- `/api/settings` GET endpoint returns rotation setting
- `/api/settings` POST endpoint accepts rotation changes
- Default rotation: 0°

## Usage Examples

### From Web Interface

1. Navigate to Settings page
2. Select rotation from dropdown
3. Click "Save Camera Settings"
4. ESP32 updates within 1 second

### From settings.json

```json
{
  "camera": {
    "rotation": 90,
    "quality": 10,
    "framesize": 6
  }
}
```

### Via Remote API

```bash
curl -X POST http://your-flask-server:5000/api/settings \
  -H "Content-Type: application/json" \
  -d '{"camera": {"rotation": 180}}'
```

ESP32 will pick up the change on next poll (within 500ms by default).

### Programmatically in ESP32 Code

```c
// After camera initialization
camera->set_rotation(camera, 90);  // Rotate 90° clockwise

// Or use settings
settings->set_camera_rotation(settings, 180);
```

## How Rotation Works

Rotation is implemented using combinations of hardware mirror and flip:

| Rotation | Implementation     | Visual Result                 |
| -------- | ------------------ | ----------------------------- |
| 0°       | hmirror=0, vflip=0 | Normal orientation            |
| 90°      | hmirror=1, vflip=0 | Rotated 90° clockwise         |
| 180°     | hmirror=1, vflip=1 | Upside down                   |
| 270°     | hmirror=0, vflip=1 | Rotated 90° counter-clockwise |

**Advantages:**

- ✅ Hardware-based (no CPU overhead)
- ✅ Works with JPEG compression
- ✅ No quality loss
- ✅ Instant effect

## Files Modified

### ESP32 Firmware:

- `main/settings_manager.h` - Added rotation field and method
- `main/settings_manager.c` - Implemented rotation getter/setter with validation
- `main/camera.h` - Added rotation method declarations
- `main/camera.c` - Implemented rotation logic
- `main/main.c` - Apply rotation from settings on boot
- `main/remote_control.c` - Handle rotation commands from server

### Flask Server:

- `templates/settings.html` - Added rotation dropdown UI
- `app.py` - Added rotation to default settings

### Documentation:

- `docs/CAMERA_ORIENTATION.md` - Technical details
- `docs/ROTATION_INTEGRATION.md` - This file

## Testing

1. **Web Interface Test:**

   - Open settings page
   - Change rotation
   - Verify image orientation on gallery page

2. **Settings Persistence Test:**

   - Set rotation to 90°
   - Restart ESP32
   - Verify rotation is still applied

3. **Remote Control Test:**

   - Send rotation command via API
   - Check ESP32 logs for "Camera rotation updated to X°"
   - Verify next captured image has correct rotation

4. **Validation Test:**
   - Try invalid value (e.g., 45)
   - Should reject with error message

## Troubleshooting

### Rotation Not Applied

- Check ESP32 logs for "Applying camera rotation from settings: X°"
- Verify settings.json contains valid rotation value
- Ensure camera initialized successfully

### Settings Not Saving

- Check SPIFFS mounted successfully
- Verify available space: `esp_spiffs_info()`
- Check for write errors in logs

### Remote Command Not Working

- Verify ESP32 polling server successfully
- Check Flask logs for incoming settings
- Confirm command queue not empty: `/api/queue-status`

## Future Enhancements

Potential improvements:

- [ ] Live preview of rotation in web interface
- [ ] Rotation presets for different mounting positions
- [ ] Auto-detect orientation using accelerometer
- [ ] Per-image rotation override

## API Reference

### ESP32 Camera Methods

```c
void set_rotation(Camera_t* self, int degrees);
void set_hmirror(Camera_t* self, int enable);
void set_vflip(Camera_t* self, int enable);
```

### Settings Manager Methods

```c
esp_err_t set_camera_rotation(SettingsManager_t* self, uint16_t rotation);
```

### Flask API Endpoints

```
GET  /api/settings           - Get current settings (includes rotation)
POST /api/settings           - Update settings (supports rotation)
GET  /api/queue-status       - Check if settings are queued
```

## Summary

Camera rotation is now fully integrated across the entire system:

- ✅ Web interface with dropdown selector
- ✅ Persistent storage in settings.json
- ✅ Remote control via API
- ✅ Hardware-based implementation (no performance impact)
- ✅ Validated input (0, 90, 180, 270 only)
- ✅ Applied on camera initialization
- ✅ Documented and tested
