# Poem Camera Flask Server

A modern Flask-based backend and frontend for the Poem Camera project. This server replaces the ESP32's built-in web server, allowing you to host images and provide a better user interface.

## Features

- 📸 **Image Upload & Gallery** - Upload images from ESP32 or web interface
- ✨ **AI Poem Generation** - Generate creative poems from images using Google Gemini AI
- 🎨 **Modern Dark Theme UI** - Beautiful responsive design
- 🚀 **Local & Cloud Ready** - Run locally or deploy to PythonAnywhere
- 📱 **Mobile Responsive** - Works great on all devices
- 🔄 **Real-time Updates** - Dynamic gallery refresh
- 🗑️ **Image Management** - View, download, and delete images
- 🔌 **REST API** - Easy integration with ESP32

## Project Structure

```
flask_server/
├── run.py                  # Main entry point (run this!)
├── app.py                  # Flask application factory
├── wsgi.py                 # WSGI config for PythonAnywhere
├── requirements.txt        # Python dependencies
├── .env.example           # Environment variables template
├── .gitignore             # Git ignore file
├── README.md              # This file
├── static/                # Static files (CSS, JS, images)
│   ├── css/
│   │   └── style.css
│   └── js/
│       └── main.js
├── templates/             # HTML templates
│   ├── index.html
│   ├── 404.html
│   └── 500.html
└── uploads/               # Uploaded images (created automatically)
    └── .gitkeep
```

## Local Installation

### Prerequisites

- Python 3.8 or higher
- pip (Python package installer)

### Setup Steps

1. **Navigate to the flask_server directory**

   ```bash
   cd flask_server
   ```

2. **Create a virtual environment (recommended)**

   ```bash
   python -m venv venv

   # On Windows:
   venv\Scripts\activate

   # On Linux/Mac:
   source venv/bin/activate
   ```

3. **Install dependencies**

   ```bash
   pip install -r requirements.txt
   ```

4. **Create environment file (optional)**

   ```bash
   copy .env.example .env
   # Edit .env with your settings
   ```

5. **Configure Gemini API (for poem generation)**

   - Get your API key from [Google AI Studio](https://makersuite.google.com/app/apikey)
   - Add it to your `.env` file:
     ```
     GEMINI_API_KEY=your_gemini_api_key_here
     ```

6. **Run the server**

   ```bash
   python run.py
   ```

6. **Open your browser**
   Navigate to: `http://127.0.0.1:5000`

## PythonAnywhere Deployment

PythonAnywhere offers a free tier perfect for hosting this application!

### Step 1: Create PythonAnywhere Account

1. Go to [www.pythonanywhere.com](https://www.pythonanywhere.com)
2. Sign up for a free "Beginner" account

### Step 2: Upload Files

Option A - Upload via Web Interface:

1. Go to the "Files" tab
2. Click "Upload a file"
3. Upload all files maintaining the folder structure

Option B - Use Git:

1. Go to "Consoles" → "Bash"
2. Clone your repository:
   ```bash
   git clone <your-repo-url>
   cd flask_server
   ```

### Step 3: Install Dependencies

1. Open a Bash console
2. Navigate to your project directory
3. Create a virtual environment:
   ```bash
   mkvirtualenv --python=/usr/bin/python3.10 flask_server
   pip install -r requirements.txt
   ```

### Step 4: Configure Web App

1. Go to the "Web" tab
2. Click "Add a new web app"
3. Choose "Manual configuration"
4. Select Python 3.10
5. In the "Code" section:

   - **Source code**: `/home/yourusername/flask_server`
   - **WSGI configuration file**: Click on the link and replace contents with:

     ```python
     import sys
     import os

     project_home = '/home/yourusername/flask_server'
     if project_home not in sys.path:
         sys.path.insert(0, project_home)

     os.environ['FLASK_ENV'] = 'production'

     from app import create_app
     application = create_app()
     ```

6. In the "Virtualenv" section:

   - Enter: `/home/yourusername/.virtualenvs/flask_server`

7. In the "Static files" section, add:

   - URL: `/static/`
   - Directory: `/home/yourusername/flask_server/static`

8. Click "Reload" at the top of the page

### Step 5: Access Your App

Your app will be available at:
`https://yourusername.pythonanywhere.com`

### Troubleshooting PythonAnywhere

- **Error logs**: Check the "Error log" link on the Web tab
- **Reload**: Always reload after making changes
- **Permissions**: Ensure the `uploads` folder has write permissions
- **Virtual environment**: Make sure it's activated when installing packages

## ESP32 Integration

### Sending Images from ESP32

Update your ESP32 code to send images to your Flask server:

```c
// POST image to Flask server
esp_http_client_config_t config = {
    .url = "http://your-server-url/api/capture",
    .method = HTTP_METHOD_POST,
};

esp_http_client_handle_t client = esp_http_client_init(&config);

// Set headers
esp_http_client_set_header(client, "Content-Type", "multipart/form-data; boundary=----Boundary");

// Prepare multipart form data
char *post_data = /* construct multipart form data with image */;
esp_http_client_set_post_field(client, post_data, strlen(post_data));

esp_http_client_perform(client);
esp_http_client_cleanup(client);
```

### API Endpoints

| Method | Endpoint                        | Description                 |
| ------ | ------------------------------- | --------------------------- |
| GET    | `/`                             | Main web interface          |
| POST   | `/upload`                       | Upload image (web form)     |
| POST   | `/api/capture`                  | Upload image (ESP32)        |
| GET    | `/images`                       | List all images (JSON)      |
| GET    | `/images/<filename>`            | Get specific image          |
| POST   | `/delete/<filename>`            | Delete image                |
| POST   | `/api/generate-poem/<filename>` | Generate poem for image     |
| GET    | `/health`                       | Health check                |

### Example: Upload from ESP32

```bash
curl -X POST -F "image=@photo.jpg" http://your-server-url/api/capture
```

## Configuration

### Environment Variables

Create a `.env` file:

```env
FLASK_ENV=development
SECRET_KEY=your-secret-key-here
PORT=5000
GEMINI_API_KEY=your_gemini_api_key_here
```

### Settings

- **MAX_CONTENT_LENGTH**: 16MB (change in `app.py`)
- **ALLOWED_EXTENSIONS**: jpg, jpeg, png, gif (change in `app.py`)
- **UPLOAD_FOLDER**: `uploads/` directory

## Development

### Running in Debug Mode

```bash
# Set environment variable
set FLASK_ENV=development  # Windows
export FLASK_ENV=development  # Linux/Mac

python run.py
```

### Testing

```bash
# Test health endpoint
curl http://127.0.0.1:5000/health

# Upload test image
curl -X POST -F "image=@test.jpg" http://127.0.0.1:5000/upload

# List images
curl http://127.0.0.1:5000/images
```

## Security Notes

⚠️ **Important for Production:**

1. Change the `SECRET_KEY` in production
2. Use HTTPS (PythonAnywhere provides this automatically)
3. Implement authentication for image deletion if needed
4. Set appropriate file size limits
5. Validate file types on server side (already implemented)

## Troubleshooting

### Port Already in Use

```bash
# Windows
netstat -ano | findstr :5000
taskkill /PID <process_id> /F

# Linux/Mac
lsof -ti:5000 | xargs kill -9
```

### Permission Denied on Uploads

```bash
# Linux/Mac
chmod 755 uploads/

# Windows - Right-click uploads folder → Properties → Security
```

### Module Not Found

```bash
pip install -r requirements.txt --force-reinstall
```

## License

This project is part of the Poem Camera project.

## Support

For issues or questions:

1. Check the error logs
2. Verify all dependencies are installed
3. Ensure the uploads folder exists and is writable

## Changelog

### Version 1.0.0

- Initial release
- Image upload and gallery
- ESP32 API integration
- PythonAnywhere support
- Responsive dark theme UI
