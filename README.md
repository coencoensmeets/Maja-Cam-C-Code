# ESP32-S3 Camera Project

A WiFi-enabled camera project for ESP32-S3 with web streaming interface, WiFi provisioning portal, and JSON-based configuration management.(See the README.md file in the upper level 'examples' directory for more information about examples.)

## Features
This is the simplest buildable example. The example is used by command `idf.py create-project`

that copies the project to user specified path and set it's name. For more information follow the [docs page](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project)

- **OV2640 Camera Support** - Live video streaming over HTTP

- **WiFi Provisioning Portal** - Easy setup via captive portal when no credentials configured

- **JSON Configuration** - Persistent settings stored in SPIFFS filesystem

- **Web Server** - Access camera stream from any browser on your network## How to use example

- **LED Status Indicators** - Visual feedback for WiFi and system statusWe encourage the users to use the example as a template for the new projects.

A recommended way is to follow the instructions on a [docs page](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project).

## Hardware Requirements

## Example folder contents

- ESP32-S3 development board

- OV2640 camera moduleThe project **sample_project** contains one source file in C language [main.c](main/main.c). The file is located in folder [main](main).

- USB cable for programming

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt`

## Quick Startfiles that provide set of directives and instructions describing the project's source files and targets

(executable, library, or both).

### 1. Clone and Setup

Below is short explanation of remaining files in the project folder.

````bash

git clone <repository-url>```

cd Poem_cam├── CMakeLists.txt

```├── main

│   ├── CMakeLists.txt

### 2. Configure WiFi Credentials│   └── main.c

└── README.md                  This is the file you are currently reading

Create your WiFi credentials file:```

Additionally, the sample project contains Makefile and component.mk files, used for the legacy Make based build system.

```bashThey are not used or needed when building with CMake and idf.py.

# Copy the template
cp data/secrets.json.example data/secrets.json

# Edit data/secrets.json with your WiFi credentials
````

**data/secrets.json:**

```json
{
  "wifi_ssid": "YourWiFiNetwork",
  "wifi_password": "YourPassword"
}
```

### 3. Build and Flash

```bash
# Build, flash firmware and SPIFFS data in one command
idf.py build flash monitor

# Or flash everything including partition table
idf.py fullclean build flash monitor
```

The `idf.py flash` command automatically includes the SPIFFS filesystem with your JSON configuration files.

### 4. Access the Camera

- **With WiFi configured**: Connect to your network and access `http://<ESP32-IP>`
- **Without WiFi configured**: Connect to `ESP32-Camera-Setup` WiFi network and navigate to `http://192.168.4.1` to configure

## WiFi Configuration Options

### Option 1: Pre-configured Credentials (Recommended)

Edit `data/secrets.json` before flashing:

```json
{
  "wifi_ssid": "YourNetwork",
  "wifi_password": "YourPassword"
}
```

Then flash with `idf.py build flash monitor` - credentials are automatically included.

### Option 2: Provisioning Portal (First-time Setup)

If no credentials are configured:

1. ESP32 creates WiFi network: **ESP32-Camera-Setup**
2. Connect to this network (password: `setupesp32`)
3. Browser automatically opens to `http://192.168.4.1`
4. Enter your WiFi credentials in the web form
5. ESP32 saves credentials and reboots
6. Connects to your network automatically

## Configuration Files

### secrets.json (WiFi Credentials)

```json
{
  "wifi_ssid": "YourNetwork",
  "wifi_password": "YourPassword"
}
```

### settings.json (Camera & System Settings)

```json
{
  "camera_resolution": 13,
  "camera_quality": 12,
  "camera_brightness": 0,
  "camera_contrast": 0,
  "camera_saturation": 0,
  "system_led_enabled": true,
  "system_hostname": "esp32-camera"
}
```

## Project Structure

```
Poem_cam/
├── main/
│   ├── main.c                 # Application entry point
│   ├── camera.c/h             # Camera driver
│   ├── web_server.c/h         # HTTP server for streaming
│   ├── wifi_manager.c/h       # WiFi connection management
│   ├── wifi_provisioning.c/h  # Captive portal for setup
│   ├── settings_manager.c/h   # JSON configuration management
│   └── led.c/h                # LED status indicators
├── data/
│   ├── secrets.json           # Your WiFi credentials (gitignored)
│   ├── secrets.json.example   # Template
│   ├── settings.json          # Camera/system configuration
│   └── settings.json.example  # Template
├── docs/
│   ├── API_REFERENCE.md       # API documentation
│   ├── ARCHITECTURE.md        # System architecture
│   ├── ORGANIZATION.md        # Code organization
│   └── SPIFFS.md              # SPIFFS filesystem details
├── partitions.csv             # Partition table (includes 960KB SPIFFS)
└── CMakeLists.txt             # Build configuration
```

## Common Commands

```bash
# Full clean build and flash
idf.py fullclean build flash monitor

# Quick rebuild and flash
idf.py build flash monitor

# Just monitor serial output
idf.py monitor

# Flash only bootloader
idf.py bootloader-flash

# Erase entire flash
idf.py erase-flash

# Clean build artifacts
idf.py clean
```

## Troubleshooting

### WiFi Connection Issues

**Problem:** ESP32 not connecting to WiFi

- Check `data/secrets.json` has correct credentials
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Check signal strength - move ESP32 closer to router
- Use provisioning portal to reconfigure

**Problem:** Provisioning portal not appearing

- Look for `ESP32-Camera-Setup` WiFi network
- Connect manually if auto-redirect fails
- Navigate to `http://192.168.4.1` in browser
- Disable mobile data on phone to prevent routing conflicts

### Camera Issues

**Problem:** Camera initialization failed

- Check camera ribbon cable connections
- Verify camera is OV2640 model
- Ensure sufficient power supply (camera draws significant current)
- Check GPIO pin configuration in `camera.h`

**Problem:** Poor image quality

- Adjust `camera_quality` in `settings.json` (lower = better, 10-12 recommended)
- Increase `camera_resolution` for higher resolution
- Adjust lighting conditions
- Clean camera lens

### Build Issues

**Problem:** SPIFFS partition not found

- Ensure `partitions.csv` is present
- Run `idf.py fullclean build flash`
- Check partition table in build output

**Problem:** JSON files not loading

- Verify files exist in `data/` folder
- Check file permissions
- Ensure SPIFFS was flashed (automatic with `idf.py flash`)
- Monitor serial output for filesystem errors

### Memory Issues

**Problem:** Out of memory errors

- Reduce image resolution in `settings.json`
- Close other browser tabs/connections
- Check for memory leaks in serial monitor
- Reduce frame buffer allocation in camera config

## LED Status Indicators

- **Blinking slowly** - Connecting to WiFi
- **Solid on** - WiFi connected successfully
- **Blinking rapidly** - Error condition
- **Off** - Disabled in settings or system idle

## Security Notes

- `data/secrets.json` is gitignored by default - your credentials stay private
- Change provisioning portal password in `wifi_provisioning.c` (default: `setupesp32`)
- Web server has no authentication by default - add if needed
- Consider static IP configuration for production deployments

## Development

See documentation in `docs/` folder:

- `API_REFERENCE.md` - Detailed API documentation
- `ARCHITECTURE.md` - System design and component interaction
- `ORGANIZATION.md` - Code structure and patterns
- `SPIFFS.md` - Filesystem and data storage details

## License

[Your License Here]

## Contributing

[Your Contributing Guidelines Here]
