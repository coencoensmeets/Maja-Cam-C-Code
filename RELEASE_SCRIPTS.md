# Release Script

Automated firmware build and release management for ESP32.

## `release.bat` - All-in-One Firmware Manager

**Single script with interactive menus for all firmware operations**

### Features

- 🔧 **Build firmware only** - Compile without releasing
- 📦 **Create official releases** - Versioned production releases (vX.Y.Z)
- 🧪 **Create testing builds** - Development builds with testing tag
- 📱 **Flash to device** - Auto-detect COM ports and flash
- 🧹 **Clean builds** - Clear build directory
- 📊 **Monitor device** - View serial output logs
- 🎯 **Interactive menus** - Easy navigation and operation

### Quick Start

```cmd
release.bat
```

### Main Menu

```
========================================
  ESP32 Firmware Manager
========================================

Current Version: v1.0.0

What would you like to do?

  [1] Build firmware only
  [2] Create official release (vX.Y.Z)
  [3] Create testing build
  [4] Flash firmware to device
  [5] Clean build directory
  [6] Monitor device logs
  [0] Exit
```

---

## Menu Options

### [1] Build Firmware Only

Compiles firmware without creating a release.

**Process:**
1. Runs `idf.py build`
2. Shows firmware size
3. Options:
   - Flash to device
   - Copy to releases folder
   - Return to menu

**Use when:**
- Testing code changes
- Verifying compilation
- Preparing for manual flash

---

### [2] Create Official Release

Creates a production release with version tag.

**Process:**
1. Shows current version
2. Asks for new version (X.Y.Z format)
3. Validates version format
4. Collects release notes
5. Updates `FIRMWARE_VERSION` in code
6. Builds firmware
7. Creates Git commit and tag
8. Pushes to GitHub
9. Prepares release folder
10. Opens GitHub release page

**Example:**
```
Current version: v1.0.0
Enter new version: 1.0.1

Enter release notes:
- Fixed camera bug
- Improved OTA stability
^Z

Building...
✓ Build successful!
✓ Committed and tagged v1.0.1
✓ Pushed to GitHub
```

**Output:**
- `releases/v1.0.1/firmware.bin`
- `releases/v1.0.1/release-info.txt`
- Git tag: `v1.0.1`

---

### [3] Create Testing Build

Creates a development/testing build.

**Process:**
1. Auto-generates timestamp version
2. Collects testing notes
3. Updates version to "testing"
4. Builds firmware
5. Creates/updates testing tag
6. Force-pushes to GitHub
7. Opens GitHub release page

**Example:**
```
Testing version: testing-20251127-143022
Tag: testing

Enter testing notes:
- New experimental feature
- WIP: Menu redesign
^Z
```

**Output:**
- `releases/testing/firmware.bin`
- Git tag: `testing` (force-updated)

---

### [4] Flash Firmware to Device

Flashes compiled firmware to ESP32.

**Process:**
1. Auto-detects COM ports
2. Shows available ports or allows manual entry
3. Runs `idf.py flash`
4. Optionally starts monitor

**Example:**
```
Detecting COM ports...
  [1] COM3
  [2] COM5

Select port: 1

Flashing to COM3...
✓ Flash successful!

Monitor device? [Y/N]: Y
```

**Smart port detection:**
- Single port: Auto-selects
- Multiple ports: Menu selection
- No ports: Manual entry

---

### [5] Clean Build Directory

Removes all build artifacts.

**Process:**
1. Runs `idf.py fullclean`
2. Clears build cache
3. Returns to menu

**Use when:**
- Build errors occur
- Changing ESP-IDF versions
- Starting fresh build

---

### [6] Monitor Device Logs

Connects to device serial output.

**Process:**
1. Detects COM ports
2. Runs `idf.py monitor`
3. Shows live logs

**Use for:**
- Debugging
- Viewing OTA updates
- Checking system status

**Exit monitor:** `Ctrl+]`

---

## Version Management

### Version Format

**Official Releases:** `vX.Y.Z`
- `v1.0.0` - Initial release
- `v1.1.0` - New features
- `v1.0.1` - Bug fixes

**Testing Builds:** `testing-YYYYMMDD-HHMMSS`
- `testing-20251127-143022`

### Version Rules

- **MAJOR** (X): Breaking changes
- **MINOR** (Y): New features, backward compatible  
- **PATCH** (Z): Bug fixes only

---

## Workflows

### Quick Build & Flash

```
1. Run: release.bat
2. Select: [1] Build firmware only
3. Wait for build
4. Choose: Flash to device
5. Select COM port
6. Done!
```

### Create Release

```
1. Run: release.bat
2. Select: [2] Create official release
3. Enter version: 1.0.1
4. Enter release notes + Ctrl+Z
5. Wait for automation
6. Browser opens to GitHub
7. Upload firmware.bin
8. Publish release
```

### Testing Build

```
1. Run: release.bat
2. Select: [3] Create testing build
3. Enter notes + Ctrl+Z
4. Wait for automation
5. Upload to GitHub as pre-release
```

### Flash & Monitor

```
1. Run: release.bat
2. Select: [4] Flash firmware to device
3. Select COM port
4. Choose Y to monitor
5. View logs in real-time
```

---

## Requirements

### Software
- **ESP-IDF** (v5.0+)
- **Git** (configured with GitHub)
- **PowerShell** (Windows built-in)

### Setup

1. Open **ESP-IDF Command Prompt**
2. Navigate to project:
   ```cmd
   cd C:\Users\coenc\Workspace\Code\Poem-Camera\C-Code\Poem_cam
   ```
3. Run script:
   ```cmd
   release.bat
   ```

---

## Troubleshooting

### "ESP-IDF not found"
**Fix:** Run from ESP-IDF Command Prompt, not regular CMD

### "Build failed"
**Fix:** 
- Check error messages
- Try option [5] Clean build
- Fix code issues and retry

### "Tag already exists"
**Fix:**
```cmd
git tag -d v1.0.0
git push origin :refs/tags/v1.0.0
```

### "No COM ports detected"
**Fix:**
- Check USB connection
- Install CP210x drivers
- Enter port manually

### "Push failed"
**Fix:**
```cmd
git config --global credential.helper manager-core
```

---

## File Structure

```
Poem_cam/
├── release.bat              # Main script
├── releases/
│   ├── v1.0.0/
│   │   ├── firmware.bin
│   │   └── release-info.txt
│   ├── v1.0.1/
│   │   └── firmware.bin
│   └── testing/
│       └── firmware.bin
├── build/
│   └── Poem_cam.bin
└── main/
    └── ota_manager.h        # Version updated here
```

---

## Tips

### Faster Workflow
- Keep ESP-IDF terminal open
- Use SSH for Git (no passwords)
- Test builds before releasing

### Best Practices
- Always test firmware locally first
- Write clear release notes
- Use semantic versioning
- Tag testing builds as pre-release

### Keyboard Shortcuts in Monitor
- `Ctrl+]` - Exit monitor
- `Ctrl+T` then `Ctrl+H` - Help
- `Ctrl+T` then `Ctrl+R` - Reset device

---

## One Script, Everything You Need! 🚀

## Scripts

### `create_release.bat` - Full Release Builder

**Comprehensive release automation with all features**

**Features:**
- Interactive release type selection (Release/Testing)
- Automatic version management in code
- Firmware building with validation
- Git commit, tag, and push automation
- Release notes collection
- Binary preparation for GitHub
- Browser integration

**Usage:**
```cmd
create_release.bat
```

**Process:**
1. Select release type:
   - **[1] Official Release** - For production (creates vX.Y.Z tag)
   - **[2] Testing Build** - For development (updates testing tag)

2. Enter version (Release mode) or auto-generate (Testing mode)

3. Enter release notes (Ctrl+Z then Enter when done)

4. Script automatically:
   - Updates `FIRMWARE_VERSION` in `ota_manager.h`
   - Cleans and builds firmware
   - Commits version changes
   - Creates/updates Git tag
   - Pushes to GitHub
   - Prepares release folder with binary
   - Opens GitHub release page

5. Upload `firmware.bin` to GitHub release

**Output Location:**
- `releases/vX.Y.Z/firmware.bin` (Release mode)
- `releases/testing/firmware.bin` (Testing mode)

---

### `quick_release.bat` - Fast Release Script

**Minimal prompts for experienced users**

**Features:**
- Shows current version
- Quick version input
- Automatic build, commit, tag, push
- Binary preparation
- GitHub browser integration

**Usage:**
```cmd
quick_release.bat
```

**Process:**
1. Displays current version from code
2. Prompts for new version (e.g., `1.0.1`)
3. Automatically:
   - Updates version in code
   - Builds firmware
   - Commits and tags
   - Pushes to GitHub
   - Prepares release folder
   - Opens GitHub release page

**Output Location:**
- `releases/vX.Y.Z/firmware.bin`

---

## Requirements

### Software
- **ESP-IDF** - ESP-IDF Command Prompt environment
- **Git** - Configured with GitHub credentials
- **PowerShell** - For text replacement (built into Windows)

### Setup
1. Open **ESP-IDF Command Prompt** from Start Menu
2. Navigate to project directory:
   ```cmd
   cd C:\Users\coenc\Workspace\Code\Poem-Camera\C-Code\Poem_cam
   ```
3. Run the desired script

### Git Configuration
Ensure Git is configured with your credentials:
```cmd
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"
```

For easier pushing, consider SSH keys or credential manager.

---

## Workflow Examples

### Creating a Production Release

```cmd
# 1. Run the full release script
create_release.bat

# 2. Select option 1 (Official Release)
# 3. Enter version: 1.2.0
# 4. Enter release notes:
#    - Fixed camera rotation bug
#    - Improved OTA error handling
#    - Added new menu option
#    [Ctrl+Z then Enter]

# 5. Wait for build and push
# 6. Browser opens to GitHub
# 7. Upload firmware.bin
# 8. Click "Publish release"
```

### Creating a Testing Build

```cmd
# 1. Run the full release script
create_release.bat

# 2. Select option 2 (Testing Build)
# 3. Enter release notes
# 4. Wait for build and push
# 5. Upload to GitHub release with "testing" tag
# 6. Mark as "Pre-release"
```

### Quick Patch Release

```cmd
# 1. Run quick release
quick_release.bat

# 2. Enter version: 1.0.1
# 3. Wait for automation
# 4. Upload to GitHub
```

---

## Release Types Explained

### Official Release (vX.Y.Z)

**When to use:**
- Stable, tested firmware ready for production
- New features complete and verified
- Bug fixes validated

**Version Format:** `vMAJOR.MINOR.PATCH`
- **MAJOR**: Breaking changes (v2.0.0)
- **MINOR**: New features, backward compatible (v1.1.0)
- **PATCH**: Bug fixes (v1.0.1)

**Examples:**
- `v1.0.0` - Initial release
- `v1.1.0` - Added thermal printer support
- `v1.1.1` - Fixed printer bug
- `v2.0.0` - Complete UI redesign

**Device Behavior:**
- Devices on "Release" channel will detect update
- Auto-check will notify if enabled
- Users see version in logs

### Testing Build

**When to use:**
- Development/beta testing
- Pre-release validation
- Sharing experimental features

**Version Format:** `testing-YYYYMMDD-HHMMSS`

**Examples:**
- `testing-20251127-143022` - Testing build from Nov 27, 2025

**Device Behavior:**
- Devices on "Testing" channel will detect update
- Always shows update available
- Overwrites previous testing build

**Settings Configuration:**
```json
{
  "ota": {
    "update_channel": 1,  // 0=Release, 1=Testing
    "auto_check": false
  }
}
```

---

## Troubleshooting

### "ESP-IDF not found"
**Solution:** Run from ESP-IDF Command Prompt, not regular CMD

### "Build failed"
**Solution:** 
1. Check error messages
2. Fix code issues
3. Run script again (it will resume)

### "Failed to push tag"
**Solution:**
```cmd
# If tag already exists, delete it first
git tag -d v1.0.0
git push origin :refs/tags/v1.0.0

# Then run script again
```

### "Git authentication failed"
**Solution:**
```cmd
# Use credential manager or SSH
git config --global credential.helper manager-core

# Or set up SSH key
# https://docs.github.com/en/authentication/connecting-to-github-with-ssh
```

### Binary not created
**Solution:**
- Check build output for errors
- Verify partition table configuration
- Ensure sufficient disk space

---

## GitHub Release Checklist

After running the script:

- [ ] Go to GitHub releases page (or browser auto-opens)
- [ ] Verify tag is selected
- [ ] Add release title (e.g., "Release 1.0.0")
- [ ] Paste release notes
- [ ] Attach `firmware.bin` from releases folder
- [ ] For testing: Check "This is a pre-release"
- [ ] Click "Publish release"
- [ ] Verify binary is downloadable
- [ ] Test OTA update on a device

---

## Tips

### Faster Releases
1. Use `quick_release.bat` for patches
2. Prepare release notes in advance
3. Keep ESP-IDF terminal open
4. Use SSH for Git (no password prompts)

### Version Strategy
- Increment PATCH for bug fixes (1.0.0 → 1.0.1)
- Increment MINOR for new features (1.0.1 → 1.1.0)
- Increment MAJOR for breaking changes (1.1.0 → 2.0.0)

### Testing Before Release
```cmd
# Flash to device before releasing
idf.py -p COM3 flash monitor

# Test all features
# Verify version shows correctly
# Check for memory leaks
```

### Rollback a Release
```cmd
# Delete tag locally and remotely
git tag -d v1.0.0
git push origin :refs/tags/v1.0.0

# Delete GitHub release
# (via GitHub web interface)

# Revert version in code
git checkout HEAD~1 -- main/ota_manager.h
```

---

## Advanced: CI/CD Integration

For automated releases on every tag push, see `docs/OTA_UPDATE_SYSTEM.md` for GitHub Actions workflow example.

---

## File Structure

After running scripts:

```
Poem_cam/
├── create_release.bat       # Full featured release script
├── quick_release.bat         # Fast release script
├── releases/                 # Release binaries
│   ├── v1.0.0/
│   │   ├── firmware.bin
│   │   └── release-info.txt
│   ├── v1.0.1/
│   │   ├── firmware.bin
│   │   └── release-info.txt
│   └── testing/
│       ├── firmware.bin
│       └── release-info.txt
├── build/
│   └── Poem_cam.bin          # Build output
└── main/
    └── ota_manager.h         # Version updated here
```

---

## Support

For issues or questions:
1. Check ESP-IDF installation: `idf.py --version`
2. Check Git configuration: `git config --list`
3. Review build errors carefully
4. Ensure internet connection for GitHub push
5. Verify GitHub repository access

---

**Remember:** Always test firmware locally before releasing to production devices!
