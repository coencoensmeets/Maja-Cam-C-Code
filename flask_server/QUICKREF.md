# Quick Reference - Poem Camera Flask Server

## 🚀 Quick Start (Local)

### Windows

```cmd
cd flask_server
start.bat
```

### Linux/Mac

```bash
cd flask_server
chmod +x start.sh
./start.sh
```

### Manual Start

```bash
cd flask_server
python -m venv venv
venv\Scripts\activate        # Windows
source venv/bin/activate     # Linux/Mac
pip install -r requirements.txt
python run.py
```

**Access**: http://127.0.0.1:5000

---

## 📡 API Endpoints

| Method | Endpoint         | Description        | Usage       |
| ------ | ---------------- | ------------------ | ----------- |
| GET    | `/`              | Web interface      | Browser     |
| POST   | `/upload`        | Upload (form)      | Web form    |
| POST   | `/api/capture`   | Upload (ESP32)     | ESP32       |
| GET    | `/images`        | List images (JSON) | API         |
| GET    | `/images/<name>` | Get image          | Direct link |
| POST   | `/delete/<name>` | Delete image       | JavaScript  |
| GET    | `/health`        | Health check       | Monitoring  |

---

## 🔧 Configuration

### Environment Variables (.env)

```env
FLASK_ENV=development
SECRET_KEY=your-secret-key
PORT=5000
```

### Application Settings (app.py)

```python
MAX_CONTENT_LENGTH = 16 * 1024 * 1024  # 16MB
ALLOWED_EXTENSIONS = {'png', 'jpg', 'jpeg', 'gif'}
UPLOAD_FOLDER = 'uploads/'
```

---

## 🌐 ESP32 Integration

### Basic Upload

```c
#define SERVER_URL "http://192.168.1.100:5000/api/capture"

esp_err_t upload_image_to_server(camera_fb_t *fb) {
    // See esp32_example.c for full implementation
}
```

### Headers Required

```
Content-Type: multipart/form-data; boundary=----Boundary
```

### Form Field Name

```
image
```

---

## 📦 PythonAnywhere Deployment

### Quick Commands

```bash
# Create virtualenv
mkvirtualenv --python=/usr/bin/python3.10 flask_server_env

# Install packages
pip install -r requirements.txt

# Set virtualenv path
/home/yourusername/.virtualenvs/flask_server_env

# Set source code path
/home/yourusername/flask_server
```

### WSGI Configuration

```python
project_home = '/home/yourusername/flask_server'
sys.path.insert(0, project_home)
from app import create_app
application = create_app()
```

### Static Files

```
URL: /static/
Directory: /home/yourusername/flask_server/static
```

**Your URL**: https://yourusername.pythonanywhere.com

---

## 🐛 Troubleshooting

### Port in Use

```bash
# Windows
netstat -ano | findstr :5000
taskkill /PID <pid> /F

# Linux/Mac
lsof -ti:5000 | xargs kill -9
```

### Module Not Found

```bash
pip install -r requirements.txt --force-reinstall
```

### Uploads Permission

```bash
# Linux/Mac
chmod 755 uploads/

# Windows: Properties → Security → Full Control
```

### PythonAnywhere Errors

1. Check Error log (Web tab)
2. Verify virtualenv path
3. Check WSGI file paths
4. Click Reload button

---

## 📁 File Structure

```
flask_server/
├── run.py              # Main entry point ⭐
├── app.py              # Flask application
├── wsgi.py             # PythonAnywhere WSGI
├── requirements.txt    # Dependencies
├── .env.example        # Environment template
├── start.bat           # Windows quick start
├── start.sh            # Linux/Mac quick start
├── README.md           # Full documentation
├── DEPLOYMENT_GUIDE.md # PythonAnywhere guide
├── esp32_example.c     # ESP32 integration code
├── static/
│   ├── css/style.css
│   └── js/main.js
├── templates/
│   ├── index.html
│   ├── 404.html
│   └── 500.html
└── uploads/            # Image storage
```

---

## 🧪 Testing

### Health Check

```bash
curl http://127.0.0.1:5000/health
```

### Upload Test

```bash
curl -X POST -F "image=@test.jpg" http://127.0.0.1:5000/upload
```

### List Images

```bash
curl http://127.0.0.1:5000/images
```

---

## 🔐 Security Checklist

- [ ] Change SECRET_KEY in production
- [ ] Enable HTTPS (automatic on PythonAnywhere)
- [ ] Validate file types (✅ implemented)
- [ ] Limit file size (✅ 16MB max)
- [ ] Use strong passwords
- [ ] Regular backups

---

## 📊 Monitoring

### Local Logs

Check terminal output

### PythonAnywhere Logs

- Error log: Web tab → Error log
- Server log: Web tab → Server log
- Access log: Web tab → Access log

---

## 🎯 Common Tasks

### Add New Dependency

```bash
pip install package-name
pip freeze > requirements.txt
```

### Change Port

```env
# In .env file
PORT=8080
```

### Enable Debug Mode

```env
FLASK_ENV=development
```

### Clear Uploads

```bash
# Windows
del /Q uploads\*.*

# Linux/Mac
rm uploads/*
```

---

## 📱 Mobile Testing

### On Same Network

1. Find your local IP: `ipconfig` (Windows) or `ifconfig` (Linux/Mac)
2. Access: `http://YOUR-IP:5000`

---

## 🆘 Need Help?

1. Check README.md for detailed documentation
2. Check DEPLOYMENT_GUIDE.md for PythonAnywhere steps
3. Look at esp32_example.c for ESP32 integration
4. Review error logs
5. Check GitHub issues (if applicable)

---

## ✅ Success Indicators

**Local:**

- ✅ Server starts without errors
- ✅ Can access http://127.0.0.1:5000
- ✅ CSS styles load correctly
- ✅ Can upload images
- ✅ Images appear in gallery

**PythonAnywhere:**

- ✅ No errors in error log
- ✅ HTTPS URL accessible
- ✅ Static files load
- ✅ Can upload from web and ESP32
- ✅ /health returns JSON

---

**Pro Tip**: Always test locally before deploying to PythonAnywhere!

**Last Updated**: 2025
