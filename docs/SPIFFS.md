# SPIFFS Data Folder

This folder contains JSON files that will be uploaded to the ESP32's SPIFFS filesystem.

## Files

### `secrets.json` (gitignored)
Contains your actual WiFi credentials. This file is **not committed to git**.

**Copy from example:**
```bash
cp secrets.json.example secrets.json
```

Then edit with your credentials:
```json
{
  "wifi": {
    "ssid": "YourWiFiNetwork",
    "password": "YourWiFiPassword",
    "configured": true
  }
}
```

### `settings.json`
Contains camera and system settings. Safe to commit (no sensitive data).

## Automatic Upload with ESP-IDF ⭐ 

**The contents of this folder are automatically included when you flash!**

```bash
# This command uploads firmware AND SPIFFS data
idf.py build flash monitor
```

The CMakeLists.txt is configured to automatically:
1. Generate a SPIFFS image from this `data/` folder
2. Include it in the flash process
3. Upload it to the correct partition (0x310000)

**No separate upload step needed!**

## How It Works

The project's `CMakeLists.txt` contains:
```cmake
spiffs_create_partition_image(spiffs ${CMAKE_SOURCE_DIR}/data FLASH_IN_PROJECT)
```

This tells ESP-IDF to:
- Create a SPIFFS image from the `data/` folder during build
- Include it automatically when running `idf.py flash`
- Flash it to partition `spiffs` at address 0x310000

## Workflow

### Normal Development (Recommended)
```bash
# 1. Edit credentials
notepad data\secrets.json

# 2. Build and flash (SPIFFS included!)
idf.py build flash monitor
```

### Manual SPIFFS Upload (Rarely Needed)
If you want to upload ONLY the SPIFFS data without reflashing firmware:
```bash
upload_spiffs.bat
```

Or manually:
```bash
python %IDF_PATH%\components\spiffs\spiffsgen.py 0xF0000 data build\spiffs.bin
esptool.py --chip esp32s3 --port COM3 write_flash 0x310000 build\spiffs.bin
```

### First Boot Without secrets.json

After upload, files are accessible at:
- `/spiffs/secrets.json` - WiFi credentials
- `/spiffs/settings.json` - Camera/system settings

## Testing Locally

You can modify these files and run `idf.py flash` to update configuration without recompiling firmware code.

**Example workflow:**
1. Edit `data/secrets.json` with new WiFi credentials
2. Run `idf.py flash` (or `idf.py build flash` if you want to rebuild)
3. Reset ESP32 or it will reset automatically
4. It will use new credentials

## Important Notes

⚠️ **Security**: The `secrets.json` file is:
- **NOT encrypted** by default
- Readable by anyone with physical access to the flash
- For production, enable ESP32 Flash Encryption

✅ **Git**: 
- `secrets.json` is gitignored (safe)
- `secrets.json.example` is committed (template)
- `settings.json` is safe to commit (no passwords)

## Partition Info

- **Offset**: 0x310000
- **Size**: 0xF0000 (960 KB)
- **Type**: SPIFFS

See `partitions.csv` for full partition table.

## Troubleshooting

### Files not found after upload
- Check partition table matches: `idf.py partition-table`
- Verify offset: 0x310000
- Ensure SPIFFS size: 0xF0000

### Upload fails
- Make sure ESP32 is in bootloader mode
- Check COM port number
- Try: `idf.py erase-flash` first

### Settings not persisting
- SPIFFS may not be mounted
- Check logs for "SPIFFS: X KB total"
- Try re-uploading data folder
