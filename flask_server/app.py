"""
Flask Application Factory
"""
import os
from flask import Flask, render_template, request, jsonify, send_from_directory
from werkzeug.utils import secure_filename
from datetime import datetime
import json
import google.generativeai as genai
from PIL import Image

# Global command queue
command_queue = {"command": "none", "settings": None}

# Configure Gemini API
GEMINI_API_KEY = os.environ.get('GEMINI_API_KEY', '')
if GEMINI_API_KEY:
    genai.configure(api_key=GEMINI_API_KEY)

def create_app():
    """Create and configure the Flask application"""
    app = Flask(__name__)
    
    # Configuration
    app.config['SECRET_KEY'] = os.environ.get('SECRET_KEY', 'dev-secret-key-change-in-production')
    app.config['MAX_CONTENT_LENGTH'] = 16 * 1024 * 1024  # 16MB max file size
    app.config['UPLOAD_FOLDER'] = os.path.join(os.path.dirname(__file__), 'uploads')
    app.config['ALLOWED_EXTENSIONS'] = {'png', 'jpg', 'jpeg', 'gif'}
    
    # Ensure upload folder exists
    os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)
    
    # Helper function to check allowed file types
    def allowed_file(filename):
        return '.' in filename and \
               filename.rsplit('.', 1)[1].lower() in app.config['ALLOWED_EXTENSIONS']
    
    # Routes
    @app.route('/')
    def index():
        """Main page - displays the camera interface"""
        images = get_uploaded_images()
        return render_template('index.html', images=images)
    
    @app.route('/settings')
    def settings():
        """Settings page"""
        return render_template('settings.html')
    
    @app.route('/api/command', methods=['GET'])
    def get_command():
        """ESP32 polls this endpoint to get commands"""
        global command_queue
        
        # Return current command and settings
        response = {
            "command": command_queue["command"],
        }
        
        if command_queue["settings"]:
            response["settings"] = command_queue["settings"]
        
        # Clear the command and settings after sending once
        if command_queue["command"] == "capture":
            command_queue["command"] = "none"
        
        if command_queue["settings"]:
            command_queue["settings"] = None
        
        return jsonify(response)
    
    @app.route('/api/trigger-capture', methods=['POST'])
    def trigger_capture():
        """Queue a capture command for ESP32 to pick up"""
        global command_queue
        command_queue["command"] = "capture"
        
        return jsonify({
            'success': True,
            'message': 'Capture command queued. ESP32 will capture within 1 second.'
        })
    
    @app.route('/api/settings', methods=['GET'])
    def get_settings():
        """Get current queued settings (for UI display)"""
        global command_queue
        if command_queue["settings"]:
            return jsonify(command_queue["settings"])
        return jsonify({
            "camera": {
                "framesize": 6,
                "quality": 10,
                "brightness": 0,
                "contrast": 0,
                "saturation": 0,
                "vflip": False,
                "hmirror": False,
                "rotation": 0
            },
            "system": {
                "led_ring_brightness": 128
            },
            "server": {
                "poll_interval_ms": 500
            }
        })
    
    @app.route('/api/settings', methods=['POST'])
    def update_settings():
        """Queue settings update for ESP32 to pick up"""
        global command_queue
        settings_data = request.json
        
        command_queue["settings"] = settings_data
        
        return jsonify({
            'success': True,
            'message': 'Settings queued. ESP32 will update within 1 second.'
        })
    
    @app.route('/api/queue-status', methods=['GET'])
    def queue_status():
        """Get current queue status"""
        global command_queue
        queue_count = 0
        
        if command_queue["command"] != "none":
            queue_count += 1
        if command_queue["settings"] is not None:
            queue_count += 1
        
        return jsonify({
            'queue_count': queue_count,
            'has_command': command_queue["command"] != "none",
            'has_settings': command_queue["settings"] is not None
        })
    
    @app.route('/upload', methods=['POST'])
    def upload_image():
        """Handle image upload from ESP32 or web interface"""
        if 'image' not in request.files:
            return jsonify({'error': 'No image file provided'}), 400
        
        file = request.files['image']
        
        if file.filename == '':
            return jsonify({'error': 'No selected file'}), 400
        
        if file and allowed_file(file.filename):
            # Create filename with timestamp
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            original_filename = secure_filename(file.filename)
            name, ext = os.path.splitext(original_filename)
            filename = f"{timestamp}_{name}{ext}"
            
            filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
            file.save(filepath)
            
            return jsonify({
                'success': True,
                'filename': filename,
                'message': 'Image uploaded successfully'
            }), 200
        
        return jsonify({'error': 'Invalid file type'}), 400
    
    @app.route('/images')
    def list_images():
        """Return list of all uploaded images"""
        images = get_uploaded_images()
        return jsonify({'images': images})
    
    @app.route('/images/<filename>')
    def get_image(filename):
        """Serve an uploaded image"""
        return send_from_directory(app.config['UPLOAD_FOLDER'], filename)
    
    @app.route('/delete/<filename>', methods=['POST'])
    def delete_image(filename):
        """Delete an uploaded image"""
        filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
        if os.path.exists(filepath):
            os.remove(filepath)
            return jsonify({'success': True, 'message': 'Image deleted'})
        return jsonify({'error': 'Image not found'}), 404
    
    @app.route('/health')
    def health():
        """Health check endpoint"""
        return jsonify({
            'status': 'healthy',
            'timestamp': datetime.now().isoformat(),
            'image_count': len(get_uploaded_images())
        })
    
    @app.route('/api/capture', methods=['POST'])
    def capture_endpoint():
        """
        Endpoint for ESP32 to POST captured images
        Expects multipart/form-data with 'image' field
        """
        return upload_image()
    
    @app.route('/api/generate-poem/<filename>', methods=['POST'])
    def generate_poem(filename):
        """
        Generate a poem for an image using Gemini API
        """
        if not GEMINI_API_KEY:
            return jsonify({
                'error': 'Gemini API key not configured. Please set GEMINI_API_KEY environment variable.'
            }), 500
        
        try:
            filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
            
            if not os.path.exists(filepath):
                return jsonify({'error': 'Image not found'}), 404
            
            # Get poet style from request
            data = request.get_json() or {}
            poet_style = data.get('poet_style', 'general')
            location = data.get('location')
            
            # Build location context if available
            location_context = ""
            if location:
                lat = location.get('latitude')
                lon = location.get('longitude')
                if lat and lon:
                    # Add location context to the prompt
                    location_context = f"\n\nThe viewer is located at approximately latitude {lat:.2f}, longitude {lon:.2f}. Consider incorporating elements of this geographic location, climate, or regional characteristics subtly into the poem if relevant."
            
            # Define poet-specific prompts
            poet_prompts = {
                'general': f"""Analyze this image and create a beautiful, creative poem about it. 
                The poem should:
                - Be 8-12 lines long
                - Capture the essence, mood, and visual elements of the image
                - Use vivid imagery and poetic language
                - Have a flowing rhythm
                - Be emotionally evocative{location_context}
                
                Just return the poem, no preamble or explanation.""",
                
                'shakespeare': f"""Analyze this image and create a poem in the style of William Shakespeare.
                - Use Shakespearean language and imagery
                - Include metaphors and dramatic flair
                - Use iambic pentameter if possible
                - Be 8-12 lines long
                - Capture the dramatic essence of the image{location_context}
                
                Just return the poem, no preamble or explanation.""",
                
                'dickinson': f"""Analyze this image and create a poem in the style of Emily Dickinson.
                - Use short, concise lines with dashes
                - Focus on nature, death, immortality, or inner emotions
                - Include slant rhyme and unconventional capitalization
                - Be introspective and contemplative
                - Be 8-12 lines long{location_context}
                
                Just return the poem, no preamble or explanation.""",
                
                'frost': f"""Analyze this image and create a poem in the style of Robert Frost.
                - Use conversational yet profound language
                - Include natural imagery and rural scenes
                - Have a narrative quality with deeper meaning
                - Use clear, accessible language with hidden depths
                - Be 8-12 lines long{location_context}
                
                Just return the poem, no preamble or explanation.""",
                
                'angelou': f"""Analyze this image and create a poem in the style of Maya Angelou.
                - Use powerful, rhythmic language
                - Include themes of resilience, strength, and humanity
                - Be empowering and uplifting
                - Use vivid, sensory details
                - Be 8-12 lines long{location_context}
                
                Just return the poem, no preamble or explanation.""",
                
                'poe': f"""Analyze this image and create a poem in the style of Edgar Allan Poe.
                - Use dark, gothic, and mysterious imagery
                - Include melancholy and haunting themes
                - Use musical, rhythmic language
                - Create an atmosphere of suspense or sorrow
                - Be 8-12 lines long{location_context}
                
                Just return the poem, no preamble or explanation.""",
                
                'whitman': f"""Analyze this image and create a poem in the style of Walt Whitman.
                - Use free verse with long, flowing lines
                - Celebrate life, nature, and humanity
                - Include expansive, all-embracing imagery
                - Be bold and declarative
                - Be 8-12 lines long{location_context}
                
                Just return the poem, no preamble or explanation.""",
                
                'haiku': f"""Analyze this image and create a traditional haiku.
                - Follow the 5-7-5 syllable pattern exactly
                - Focus on nature, seasons, or a moment in time
                - Include a subtle reference to the natural world
                - Capture a fleeting moment or emotion
                - Be exactly 3 lines{location_context}
                
                Just return the haiku, no preamble or explanation."""
            }
            
            # Open the image
            img = Image.open(filepath)
            
            # Initialize Gemini model
            model = genai.GenerativeModel('gemini-2.5-flash')
            
            # Get the appropriate prompt
            prompt = poet_prompts.get(poet_style, poet_prompts['general'])
            
            response = model.generate_content([prompt, img])
            poem = response.text.strip()
            
            return jsonify({
                'success': True,
                'poem': poem,
                'filename': filename,
                'poet_style': poet_style
            })
            
        except Exception as e:
            return jsonify({
                'error': f'Failed to generate poem: {str(e)}'
            }), 500
    
    # Error handlers
    @app.errorhandler(404)
    def not_found(error):
        return render_template('404.html'), 404
    
    @app.errorhandler(500)
    def internal_error(error):
        return render_template('500.html'), 500
    
    @app.errorhandler(413)
    def too_large(error):
        return jsonify({'error': 'File too large. Maximum size is 16MB'}), 413
    
    # Helper functions
    def get_uploaded_images():
        """Get list of uploaded images with metadata"""
        images = []
        upload_folder = app.config['UPLOAD_FOLDER']
        
        if not os.path.exists(upload_folder):
            return images
        
        for filename in os.listdir(upload_folder):
            if allowed_file(filename):
                filepath = os.path.join(upload_folder, filename)
                stat = os.stat(filepath)
                images.append({
                    'filename': filename,
                    'size': stat.st_size,
                    'modified': datetime.fromtimestamp(stat.st_mtime).isoformat(),
                    'url': f'/images/{filename}'
                })
        
        # Sort by modified date, newest first
        images.sort(key=lambda x: x['modified'], reverse=True)
        return images
    
    return app
