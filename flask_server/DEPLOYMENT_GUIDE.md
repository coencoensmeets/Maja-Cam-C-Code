# PythonAnywhere Deployment Guide

This guide will walk you through deploying your Flask server to PythonAnywhere's free tier.

## Why PythonAnywhere?

- ✅ Free tier available
- ✅ No credit card required
- ✅ HTTPS included
- ✅ Easy file management
- ✅ Perfect for small projects like this

## Prerequisites

- A PythonAnywhere account (free tier)
- Your Flask server files ready to upload

## Step-by-Step Deployment

### 1. Create PythonAnywhere Account

1. Visit: https://www.pythonanywhere.com
2. Click "Start running Python online in less than a minute!"
3. Choose "Create a Beginner account" (FREE)
4. Verify your email address

### 2. Upload Your Files

**Option A: Upload via Web Interface**

1. Log in to PythonAnywhere
2. Go to the "Files" tab (top navigation)
3. You'll start in `/home/yourusername/`
4. Create a new directory: `flask_server`
5. Navigate into it
6. Upload these files one by one:
   - `run.py`
   - `app.py`
   - `wsgi.py`
   - `requirements.txt`
7. Create directories and upload:
   - Create `templates` folder → upload all HTML files
   - Create `static` folder → create `css` and `js` subfolders → upload CSS and JS files
   - Create `uploads` folder (leave empty)

**Option B: Using Git (Recommended if you have a repository)**

1. Go to "Consoles" tab
2. Click "Bash" to start a new bash console
3. Run:
   ```bash
   git clone https://github.com/yourusername/your-repo.git flask_server
   cd flask_server
   ```

### 3. Set Up Virtual Environment

1. In the Bash console, create a virtual environment:

   ```bash
   cd ~/flask_server
   mkvirtualenv --python=/usr/bin/python3.10 flask_server_env
   ```

2. The virtual environment will be automatically activated
   You'll see `(flask_server_env)` in your prompt

3. Install dependencies:

   ```bash
   pip install -r requirements.txt
   ```

   This will install:

   - Flask==3.0.0
   - Werkzeug==3.0.1
   - gunicorn==21.2.0
   - python-dotenv==1.0.0

4. Verify installation:
   ```bash
   pip list
   ```

### 4. Configure the Web App

1. Go to the "Web" tab
2. Click "Add a new web app"
3. Click "Next" on the domain name screen
4. Select "Manual configuration" (NOT Flask!)
5. Choose "Python 3.10"
6. Click "Next"

### 5. Configure WSGI File

1. On the Web tab, find "Code" section
2. Click on the WSGI configuration file link
   (something like: `/var/www/yourusername_pythonanywhere_com_wsgi.py`)

3. **DELETE** all the existing content

4. **REPLACE** with this (update `yourusername`):

   ```python
   import sys
   import os

   # Add your project directory to the sys.path
   project_home = '/home/yourusername/flask_server'
   if project_home not in sys.path:
       sys.path.insert(0, project_home)

   # Set environment variables for production
   os.environ['FLASK_ENV'] = 'production'
   os.environ['SECRET_KEY'] = 'change-this-to-something-random-and-secure'

   # Import the Flask app
   from app import create_app
   application = create_app()
   ```

5. Click "Save" (top right)

### 6. Set Virtual Environment Path

1. Still on the Web tab, scroll to "Virtualenv" section
2. Enter the path to your virtual environment:
   ```
   /home/yourusername/.virtualenvs/flask_server_env
   ```
3. Click the checkmark to confirm

### 7. Configure Static Files

1. Scroll to "Static files" section on the Web tab
2. Click "Enter path" and add:

   | URL        | Directory                                |
   | ---------- | ---------------------------------------- |
   | `/static/` | `/home/yourusername/flask_server/static` |

3. This ensures CSS, JS, and images load correctly

### 8. Set Working Directory (Important!)

1. Still on Web tab, find "Code" section
2. Set "Source code" to:
   ```
   /home/yourusername/flask_server
   ```

### 9. Enable HTTPS (Recommended)

1. On the Web tab, scroll to "Security" section
2. Toggle "Force HTTPS" to ON
3. This ensures all traffic uses secure HTTPS

### 10. Reload Your Web App

1. Scroll to the very top of the Web tab
2. Click the big green "Reload" button
3. Wait for the reload to complete (~10 seconds)

### 11. Test Your Application

1. Your app URL will be: `https://yourusername.pythonanywhere.com`
2. Click on it or visit it in a new tab
3. You should see your Poem Camera interface!

## Verifying the Deployment

### Check if Everything Works

✅ Home page loads with the gallery interface  
✅ Upload form is visible  
✅ CSS styling is applied (dark theme)  
✅ Health endpoint works: `/health`  
✅ Can upload a test image

### Troubleshooting

If something doesn't work:

**1. Check Error Logs**

- On the Web tab, click "Error log" link
- Look for Python errors or tracebacks

**2. Common Issues & Fixes**

**Issue: 502 Bad Gateway**

- Solution: Check WSGI file paths are correct
- Verify virtual environment path is correct
- Make sure you clicked Reload

**Issue: Static files (CSS/JS) not loading**

- Solution: Check Static files configuration
- Ensure paths don't have typos
- URL must start and end with `/`

**Issue: ModuleNotFoundError**

- Solution: Check virtual environment is set correctly
- Reinstall requirements:
  ```bash
  workon flask_server_env
  pip install -r requirements.txt
  ```

**Issue: Uploads folder permission denied**

- Solution: In Bash console:
  ```bash
  cd ~/flask_server
  chmod 755 uploads
  ```

**Issue: Images uploaded but not showing**

- Solution: Check uploads directory exists and is writable
- Verify application has correct permissions

## File Structure on PythonAnywhere

After deployment, your structure should look like:

```
/home/yourusername/
└── flask_server/
    ├── run.py
    ├── app.py
    ├── wsgi.py
    ├── requirements.txt
    ├── static/
    │   ├── css/
    │   │   └── style.css
    │   └── js/
    │       └── main.js
    ├── templates/
    │   ├── index.html
    │   ├── 404.html
    │   └── 500.html
    └── uploads/
        └── (uploaded images appear here)
```

## Updating Your Application

When you make changes:

1. Upload new files or edit via Files tab
2. Go to Web tab
3. Click "Reload" button
4. Clear browser cache if needed (Ctrl+F5)

## ESP32 Configuration

Update your ESP32 code to use your PythonAnywhere URL:

```c
#define SERVER_URL "https://yourusername.pythonanywhere.com/api/capture"
```

## Free Tier Limitations

PythonAnywhere free tier includes:

- ✅ 512 MB disk space
- ✅ 1 web app
- ✅ Always-on HTTPS
- ⚠️ CPU time limits (100 seconds/day)
- ⚠️ Only whitelisted external sites (your ESP32 must reach PythonAnywhere)

For this project, the free tier should be sufficient!

## Monitoring Your App

### View Logs

- **Error log**: Web tab → Error log link
- **Server log**: Web tab → Server log link
- **Access log**: Web tab → Access log link

### Check Resource Usage

- Go to "Account" tab
- View CPU usage, disk space, etc.

## Security Best Practices

1. **Change SECRET_KEY**: Use a long random string

   ```python
   os.environ['SECRET_KEY'] = 'your-very-long-random-secret-key-here'
   ```

2. **Use HTTPS**: Already configured in step 9

3. **Limit Upload Size**: Already set to 16MB in app.py

4. **Validate File Types**: Already implemented in app.py

## Backing Up Your Data

Regular backups recommended:

1. Go to Files tab
2. Navigate to `flask_server/uploads/`
3. Click "Download" next to each file
4. Or use the Bash console:
   ```bash
   tar -czf backup.tar.gz flask_server/uploads/
   ```
   Then download `backup.tar.gz` from Files tab

## Support & Resources

- PythonAnywhere Forums: https://www.pythonanywhere.com/forums/
- Help pages: https://help.pythonanywhere.com/
- Your app's Web tab has direct links to relevant help

## Success Checklist

Before considering deployment complete:

- [ ] Web app loads without errors
- [ ] All static files (CSS/JS) load correctly
- [ ] Can upload images via web interface
- [ ] Uploaded images appear in gallery
- [ ] Can view, download, and delete images
- [ ] Health endpoint returns correct JSON
- [ ] HTTPS is enabled
- [ ] ESP32 can reach the API endpoint

---

**Congratulations!** 🎉

Your Poem Camera Flask server is now live on PythonAnywhere!

Your application URL: `https://yourusername.pythonanywhere.com`
