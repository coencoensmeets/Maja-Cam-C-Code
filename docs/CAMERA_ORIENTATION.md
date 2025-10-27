# Camera Orientation Control

The camera now supports flexible orientation control through rotation, horizontal mirror, and vertical flip.

## Methods

### 1. `set_rotation(camera, degrees)`

Set camera rotation in 90-degree increments.

**Parameters:**

- `degrees`: Rotation angle (0, 90, 180, 270)
  - Any value will be rounded to nearest 90°
  - Negative values are supported (e.g., -90 = 270)

**Examples:**

```c
camera->set_rotation(camera, 0);    // Normal orientation
camera->set_rotation(camera, 90);   // 90° clockwise
camera->set_rotation(camera, 180);  // Upside down
camera->set_rotation(camera, 270);  // 90° counter-clockwise (same as -90)
```

### 2. `set_hmirror(camera, enable)`

Set horizontal mirror (flip left-right).

**Parameters:**

- `enable`: 0 = off, 1 = on

**Example:**

```c
camera->set_hmirror(camera, 1);  // Mirror horizontally
camera->set_hmirror(camera, 0);  // Normal
```

### 3. `set_vflip(camera, enable)`

Set vertical flip (flip top-bottom).

**Parameters:**

- `enable`: 0 = off, 1 = on

**Example:**

```c
camera->set_vflip(camera, 1);  // Flip vertically
camera->set_vflip(camera, 0);  // Normal
```

## Usage in Code

### In main.c (after camera initialization):

```c
// Initialize camera
if (camera->init(camera) != ESP_OK) {
    // handle error
}

// Apply desired orientation
camera->set_rotation(camera, 90);  // Rotate 90° clockwise
```

### Common Scenarios:

#### Camera Mounted Upside Down

```c
camera->set_rotation(camera, 180);
```

#### Camera Mounted Sideways (Portrait Mode)

```c
camera->set_rotation(camera, 90);   // Clockwise
// OR
camera->set_rotation(camera, 270);  // Counter-clockwise
```

#### Mirror Image (Selfie Mode)

```c
camera->set_hmirror(camera, 1);
```

#### Complete Inversion

```c
camera->set_hmirror(camera, 1);
camera->set_vflip(camera, 1);
// OR simpler:
camera->set_rotation(camera, 180);
```

## How Rotation Works

The `set_rotation()` method internally uses combinations of hmirror and vflip:

| Rotation | H-Mirror | V-Flip | Result                |
| -------- | -------- | ------ | --------------------- |
| 0°       | OFF      | OFF    | Normal                |
| 90°      | ON       | OFF    | 90° clockwise         |
| 180°     | ON       | ON     | Upside down           |
| 270°     | OFF      | ON     | 90° counter-clockwise |

## Notes

⚠️ **Important:**

- These methods only work AFTER camera initialization
- Changes take effect immediately on next capture
- Settings are applied at sensor level (hardware)
- No performance penalty for using rotation
- JPEG orientation is handled by sensor, not software

💡 **Tip:**
If you need to change orientation frequently, store the desired angle and reapply after camera reset/restart.

## Remote Control Integration

You can expose these settings through the web interface or remote control:

```json
{
  "action": "set_rotation",
  "value": 90
}
```

Or add to settings.json for persistent configuration.
