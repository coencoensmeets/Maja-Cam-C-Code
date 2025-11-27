# OTA Update System

This document explains the Over-The-Air (OTA) firmware update system for the ESP32 Camera project.

## Overview

The OTA update system allows your ESP32 device to automatically download and install firmware updates from GitHub releases without requiring a physical USB connection. Updates can be triggered through the rotary encoder menu interface.

## Features

- **GitHub Release Integration**: Fetches firmware from official GitHub releases
- **Testing Branch Support**: Option to use development/testing builds from a specific branch
- **Menu-Driven Updates**: Simple interface using the LED ring and rotary encoder
- **Auto-Check on Startup**: Optionally check for updates when the device boots
- **Visual Feedback**: LED ring animations show update progress and status
- **Secure HTTPS**: Downloads firmware over encrypted connections

## Configuration

### Settings File (`settings.json`)

Add the following OTA configuration to your `settings.json`:

```json
{
  "ota": {
    "github_owner": "coencoensmeets",
    "github_repo": "Poem_cam",
    "testing_branch": "main",
    "update_channel": 0,
    "auto_check": false
  }
}
```

**Configuration Options:**

- `github_owner`: Your GitHub username or organization name
- `github_repo`: Your repository name
- `testing_branch`: Branch name to use for testing builds (e.g., "main", "develop")
- `update_channel`: 
  - `0` = Official releases (recommended for production)
  - `1` = Testing builds from the specified branch
- `auto_check`: 
  - `false` = Manual update checks only
  - `true` = Automatically check for updates on startup

## Update Channels

### Release Channel (Recommended)

Uses official GitHub releases with semantic versioning (e.g., v1.0.0, v1.1.0).

**Advantages:**
- Stable, tested firmware
- Semantic versioning
- Release notes available
- Easy rollback to previous versions

**Setup:**
1. Set `update_channel` to `0`
2. Create GitHub releases with tags like `v1.0.0`
3. Attach the compiled `.bin` file to each release

### Testing Channel

Uses the latest build from a specific branch (configured in `testing_branch`).

**Advantages:**
- Access to latest features
- Quick iteration during development
- Test fixes before official release

**Setup:**
1. Set `update_channel` to `1`
2. Upload firmware to GitHub releases with tag "testing"
3. Set `testing_branch` to your development branch name

## Creating Releases on GitHub

### For Release Channel

1. **Build your firmware:**
   ```bash
   idf.py build
   ```
   The binary will be at: `build/Poem_cam.bin`

2. **Create a Git tag:**
   ```bash
   git tag -a v1.0.0 -m "Release version 1.0.0"
   git push origin v1.0.0
   ```

3. **Create GitHub Release:**
   - Go to your repository on GitHub
   - Click "Releases" → "Create a new release"
   - Select the tag you created (v1.0.0)
   - Add release notes describing changes
   - Attach the `Poem_cam.bin` file from the build directory
   - Click "Publish release"

### For Testing Channel

1. **Build your firmware:**
   ```bash
   idf.py build
   ```

2. **Create a "testing" release:**
   - Create a GitHub release with tag "testing"
   - Attach the firmware binary as `firmware.bin`
   - Each time you update the testing build, replace the binary in this release

## Using the OTA Update Menu

### Accessing the Update Menu

1. **Rotate the encoder** to wake up the LED ring menu
2. **Navigate to the yellow section** (OTA Update option)
3. **Press the encoder button** to check for updates

### Update Process

The LED ring will show different colors during the update:

- **Yellow (solid)**: Checking for updates...
- **Green (solid)**: Update available, downloading...
- **Blue (3 flashes)**: No update available, already up to date
- **Red (3 flashes)**: Error occurred (check WiFi connection)
- **Green (solid, then restart)**: Update successful!

### After Update

When an update is successfully installed, the device will automatically restart and boot into the new firmware version.

## Firmware Versioning

The current firmware version is defined in `ota_manager.h`:

```c
#define FIRMWARE_VERSION "v1.0.0"
```

**Update this version number** when building new firmware releases to ensure proper version comparison.

### Version Format

Use semantic versioning: `vMAJOR.MINOR.PATCH`

- **MAJOR**: Breaking changes or major features
- **MINOR**: New features, backward compatible
- **PATCH**: Bug fixes

Examples: `v1.0.0`, `v1.2.3`, `v2.0.0`

## Partition Table

The ESP32 must have an OTA-capable partition table. This project uses a partition table defined in `partitions.csv` with two app partitions for OTA updates.

**Minimum requirements:**
- Two OTA app partitions (ota_0 and ota_1)
- Each partition must be large enough for your firmware
- Factory partition (optional but recommended for recovery)

## Troubleshooting

### Update Check Fails

**Symptoms:** Red flashing after selecting OTA update

**Solutions:**
- Verify WiFi is connected (LED should have flashed green on startup)
- Check GitHub repository settings in `settings.json`
- Ensure repository is public or you have access permissions
- Verify internet connectivity

### No Update Available (But You Know There Is One)

**Solutions:**
- Check the `FIRMWARE_VERSION` in `ota_manager.h` matches your current build
- Verify the GitHub release tag format matches (must be `vX.Y.Z`)
- Ensure the `.bin` file is properly attached to the GitHub release
- Check that `update_channel` matches your release strategy

### Update Download Fails

**Solutions:**
- Ensure stable WiFi connection
- Check that the binary file is not corrupted
- Verify sufficient free space in OTA partition
- Try a smaller firmware size if needed

### Device Won't Boot After Update

**Solutions:**
- Reflash via USB using `idf.py flash`
- Check partition table configuration
- Verify firmware was built for correct chip type (ESP32-S3)
- Review build errors before creating the release

## Advanced: CI/CD Integration

You can automate firmware builds and releases using GitHub Actions:

```yaml
name: Build and Release Firmware

on:
  push:
    tags:
      - 'v*'

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Setup ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.0
          target: esp32s3
          
      - name: Build firmware
        run: |
          cd C-Code/Poem_cam
          idf.py build
          
      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: C-Code/Poem_cam/build/Poem_cam.bin
```

## Security Considerations

- **HTTPS Only**: All downloads use encrypted HTTPS connections
- **Certificate Validation**: ESP-IDF's certificate bundle validates GitHub's certificate
- **No Downgrade Prevention**: Currently allows downgrades (can be added if needed)
- **Public Releases**: Anyone can download your firmware from public GitHub releases

## API Reference

### OTA Manager Functions

```c
// Initialize OTA manager
esp_err_t init(OTAManager_t *self, const char *owner, const char *repo, const char *branch);

// Check if update is available
esp_err_t check_for_update(OTAManager_t *self, bool *update_available);

// Download and install update
esp_err_t perform_update(OTAManager_t *self);

// Set update channel
esp_err_t set_channel(OTAManager_t *self, ota_channel_t channel);

// Get version information
esp_err_t get_current_version(OTAManager_t *self, char *version_out);
esp_err_t get_latest_version(OTAManager_t *self, char *version_out);
```

## Files Modified/Created

- `main/ota_manager.h` - OTA manager header
- `main/ota_manager.c` - OTA manager implementation
- `main/settings_manager.h` - Added OTA settings structure
- `main/settings_manager.c` - Added OTA settings handling
- `main/main_menu.c` - Added OTA menu option (yellow)
- `main/main.c` - Integrated OTA manager
- `main/CMakeLists.txt` - Added OTA dependencies
- `data/settings.json.example` - Added OTA configuration

## Example Workflow

1. **Initial Setup:**
   - Configure `settings.json` with your GitHub repository
   - Build and flash initial firmware via USB

2. **Development:**
   - Make code changes
   - Test locally
   - Commit and push to GitHub

3. **Release:**
   - Update `FIRMWARE_VERSION` in `ota_manager.h`
   - Build firmware: `idf.py build`
   - Create Git tag: `git tag v1.1.0`
   - Push tag: `git push origin v1.1.0`
   - Create GitHub release with `build/Poem_cam.bin`

4. **Update Devices:**
   - Devices with `auto_check: true` will detect update on next boot
   - Or use menu to manually check and install

## Future Enhancements

Potential improvements for the OTA system:

- **Delta Updates**: Only download changed portions of firmware
- **Signature Verification**: Cryptographically sign releases
- **Rollback Protection**: Prevent accidental downgrades
- **Update Scheduling**: Auto-install at specific times
- **Progress Bar**: More detailed download progress
- **Changelog Display**: Show release notes before updating
- **Multi-Binary Support**: Handle multiple firmware variants

---

**Note:** Always test firmware updates thoroughly before deploying to production devices. Keep a USB cable handy for recovery if needed.
