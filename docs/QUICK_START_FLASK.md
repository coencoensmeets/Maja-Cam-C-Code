# ESP32 to Flask Server - Quick Setup

## What Changed?

Your ESP32 now uploads images to your Flask server instead of only hosting them locally!

## 3 Steps to Get Started

### 1️⃣ Update Configuration

Edit `data/settings.json`:

```json
{
  "server": {
    "upload_url": "http://YOUR-COMPUTER-IP:5000/api/capture",
    "upload_enabled": true,
    "upload_interval_seconds": 30
  }
}
```

**Replace `YOUR-COMPUTER-IP`** with your computer's local IP address:

- Windows: Run `ipconfig` (look for IPv4 Address)
- Linux/Mac: Run `ifconfig` or `ip addr`

### 2️⃣ Build and Flash

```bash
idf.py build flash monitor
```

### 3️⃣ Start Flask Server

In another terminal:

```bash
cd flask_server
python run.py
```

Then open: `http://localhost:5000`

## What You'll See

### ESP32 Logs:

```
✓ Image uploaded successfully!
✓ Scheduled upload completed successfully
```

### Flask Server:

Images appear automatically in the web gallery!

## Configuration Options

| Setting                   | Description           | Example                                 |
| ------------------------- | --------------------- | --------------------------------------- |
| `upload_url`              | Flask server endpoint | `http://192.168.1.100:5000/api/capture` |
| `upload_enabled`          | Turn on/off uploads   | `true` or `false`                       |
| `upload_interval_seconds` | Time between uploads  | `30` (30 seconds)                       |

## Quick Fixes

### Can't Connect?

- ✅ Check Flask server is running
- ✅ Verify IP address is correct
- ✅ Both devices on same WiFi network

### 404 Error?

- ✅ URL must end with `/api/capture`
- ✅ Check Flask server started correctly

### Too Slow?

- ✅ Reduce `upload_interval_seconds`
- ✅ Reduce image quality in settings

## Features

✨ **Auto Upload** - Images sent to Flask server automatically  
✨ **Configurable** - Change URL and interval in settings  
✨ **Dual Mode** - Local web server still works for direct viewing  
✨ **Reliable** - Error handling and retry logic  
✨ **Logged** - See upload status in serial monitor

## Next Steps

1. Test locally first
2. Deploy Flask server to PythonAnywhere (free hosting)
3. Update ESP32 settings to use PythonAnywhere URL
4. Enjoy cloud-hosted images! 🎉

---

**Full Documentation**: See `docs/FLASK_INTEGRATION.md`

**Flask Server Docs**: See `flask_server/README.md`
