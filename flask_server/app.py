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
command_queue = {"command": "none", "settings": None, "print_data": None}

# Global poem generation status
poem_generation_status = {"is_generating": False, "filename": None, "started_at": None}

# Global log storage (keep last 1000 logs)
log_storage = []
MAX_LOGS = 1000

# Global storage for current ESP32 settings (as reported by device)
current_esp32_settings = None

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
    
    @app.route('/logs')
    def logs_page():
        """Logs viewer page"""
        return render_template('logs.html')
    
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
        
        if command_queue["print_data"]:
            response["print_data"] = command_queue["print_data"]
        
        # Log the command being sent
        if command_queue["command"] != "none":
            print(f"[QUEUE] Sending command to ESP32: {command_queue['command']}")
            if command_queue["command"] == "print":
                print(f"[QUEUE] Print data: {command_queue['print_data']}")
        
        # Clear the command and data after sending (ESP32 has received it)
        if command_queue["command"] == "capture":
            command_queue["command"] = "none"
        elif command_queue["command"] == "print":
            command_queue["command"] = "none"
            command_queue["print_data"] = None
        
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
    
    @app.route('/api/print-poem', methods=['POST'])
    def print_poem():
        """Queue a print command for ESP32 to pick up"""
        global command_queue
        
        data = request.json
        if not data:
            return jsonify({'success': False, 'error': 'No data provided'}), 400
        
        # Queue the print command
        command_queue["command"] = "print"
        command_queue["print_data"] = {
            "title": data.get('title', 'Untitled'),
            "poet_style": data.get('poet_style', 'General'),
            "poem_text": data.get('poem_text', '')
        }
        
        # If the request contains a filename for an image, save the poem JSON next to that image
        filename = data.get('filename')
        if filename:
            try:
                upload_folder = app.config['UPLOAD_FOLDER']
                image_path = os.path.join(upload_folder, filename)
                if os.path.exists(image_path):
                    poems_json_path = os.path.splitext(image_path)[0] + '_poems.json'
                    
                    # Load existing poems
                    poems_list = []
                    if os.path.exists(poems_json_path):
                        try:
                            with open(poems_json_path, 'r') as f:
                                poems_list = json.load(f)
                        except:
                            poems_list = []
                    
                    # Create new poem object
                    import uuid
                    poet_style = command_queue['print_data']['poet_style']
                    new_poem = {
                        'id': str(uuid.uuid4()),
                        'title': command_queue['print_data']['title'],
                        'poet_style': poet_style,
                        'poem_text': command_queue['print_data']['poem_text'],
                        'generated_at': datetime.now().isoformat()
                    }
                    
                    # Check if this style already exists and replace it
                    style_exists = False
                    for i, existing_poem in enumerate(poems_list):
                        if existing_poem.get('poet_style', '').lower() == poet_style.lower():
                            poems_list[i] = new_poem
                            style_exists = True
                            break
                    
                    if not style_exists:
                        poems_list.append(new_poem)
                    
                    with open(poems_json_path, 'w') as f:
                        json.dump(poems_list, f, indent=2)
                    print(f"Saved printed poem to {poems_json_path}")
            except Exception as e:
                print(f"Failed to save printed poem for {filename}: {e}")
        
        return jsonify({
            'success': True,
            'message': 'Print command queued. ESP32 will print within 1 second.'
        })

    @app.route('/api/test-print', methods=['POST'])
    def test_print():
        """Queue a test print command for ESP32"""
        global command_queue
        
        # Queue a test print command
        command_queue["command"] = "print"
        command_queue["print_data"] = {
            "title": "Printer Test",
            "poet_style": "Test",
            "poem_text": "This is a test print.\nThe thermal printer is working!\nAll systems operational."
        }
        
        return jsonify({
            'success': True,
            'message': 'Test print queued. ESP32 will print within 1 second.'
        })
    
    @app.route('/api/settings', methods=['GET'])
    def get_settings():
        """Get current ESP32 settings (as reported by device)"""
        global command_queue, current_esp32_settings
        
        # Load ESP32 IP from config file if it exists
        config_file = os.path.join(os.path.dirname(__file__), 'config.json')
        esp32_ip = None
        if os.path.exists(config_file):
            try:
                with open(config_file, 'r') as f:
                    config = json.load(f)
                    esp32_ip = config.get('esp32_ip')
            except:
                pass
        
        # Default settings (used only if ESP32 hasn't reported yet)
        default_settings = {
            "camera": {
                "framesize": 6,
                "quality": 10,
                "brightness": 0,
                "contrast": 0,
                "saturation": 0,
                "vflip": False,
                "hmirror": False,
                "rotation": 0,
                "flash_enabled": True,
                "self_timer_enabled": True,
                "auto_print_enabled": True
            },
            "poem": {
                "style": "general"
            },
            "system": {
                "led_ring_brightness": 128
            },
            "server": {
                "poll_interval_ms": 500
            },
            "log": {
                "upload_enabled": True,
                "upload_interval_seconds": 30,
                "queue_size": 50
            },
            "esp32_ip": esp32_ip
        }
        
        # Use current ESP32 settings if available, otherwise use defaults
        if current_esp32_settings:
            settings = current_esp32_settings.copy()
            settings["esp32_ip"] = esp32_ip
        else:
            settings = default_settings
            
        return jsonify(settings)
    
    @app.route('/api/settings', methods=['POST'])
    def update_settings():
        """Queue settings update for ESP32 to pick up"""
        global command_queue
        settings_data = request.json
        
        # Save ESP32 IP to config file if provided
        if 'esp32_ip' in settings_data:
            config_file = os.path.join(os.path.dirname(__file__), 'config.json')
            try:
                config = {}
                if os.path.exists(config_file):
                    with open(config_file, 'r') as f:
                        config = json.load(f)
                config['esp32_ip'] = settings_data['esp32_ip']
                with open(config_file, 'w') as f:
                    json.dump(config, f, indent=2)
            except Exception as e:
                print(f"Failed to save ESP32 IP: {e}")
        
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
    
    @app.route('/api/current-settings', methods=['POST'])
    def receive_current_settings():
        """Receive current settings from ESP32"""
        global current_esp32_settings
        settings_data = request.json
        
        # Store the current settings as reported by ESP32
        current_esp32_settings = settings_data
        
        return jsonify({
            'success': True,
            'message': 'Current settings received'
        })
    
    @app.route('/api/poem-status', methods=['GET'])
    def get_poem_status():
        """Get current poem generation status"""
        global poem_generation_status
        return jsonify(poem_generation_status)
    
    @app.route('/api/logs', methods=['POST'])
    def receive_logs():
        """Receive logs from ESP32"""
        global log_storage
        
        data = request.json
        if not data or 'logs' not in data:
            return jsonify({'error': 'No logs provided'}), 400
        
        logs = data['logs']
        
        # Add logs to storage
        log_storage.extend(logs)
        
        # Trim to MAX_LOGS
        if len(log_storage) > MAX_LOGS:
            log_storage = log_storage[-MAX_LOGS:]
        
        print(f"[LOGS] Received {len(logs)} log entries. Total stored: {len(log_storage)}")
        
        return jsonify({
            'success': True,
            'received': len(logs),
            'total_stored': len(log_storage)
        })
    
    @app.route('/api/logs', methods=['GET'])
    def get_logs():
        """Get stored logs"""
        global log_storage
        
        # Optional: filter by last N logs
        limit = request.args.get('limit', type=int)
        
        if limit and limit > 0:
            logs = log_storage[-limit:]
        else:
            logs = log_storage
        
        return jsonify({
            'logs': logs,
            'total': len(log_storage)
        })
    
    @app.route('/api/logs/clear', methods=['POST'])
    def clear_logs():
        """Clear all stored logs"""
        global log_storage
        count = len(log_storage)
        log_storage = []
        
        return jsonify({
            'success': True,
            'cleared': count
        })
    
    @app.route('/upload', methods=['POST'])
    def upload_image():
        """Handle image upload from ESP32 or web interface"""
        global command_queue, poem_generation_status
        
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
            
            # Check if auto-print is enabled from the upload form data
            # ESP32 now sends auto_print_enabled and poem_style as form fields
            auto_print_enabled = request.form.get('auto_print_enabled', 'false').lower() == 'true'
            poem_style = request.form.get('poem_style', 'general')
            
            print(f"Upload received - auto_print_enabled={auto_print_enabled}, poem_style={poem_style}")
            
            # If auto-print is enabled, automatically generate poem and queue print command
            if auto_print_enabled and GEMINI_API_KEY:
                try:
                    # Set generation status
                    poem_generation_status["is_generating"] = True
                    poem_generation_status["filename"] = filename
                    poem_generation_status["started_at"] = datetime.now().isoformat()
                    
                    # Generate poem using helper function
                    title, poem = generate_poem_for_image(filepath, poem_style)
                    
                    # Queue the print command
                    command_queue["command"] = "print"
                    command_queue["print_data"] = {
                        "title": title,
                        "poet_style": poem_style.capitalize(),
                        "poem_text": poem
                    }
                    
                    # Save poem to file
                    poem_count = save_poem_to_file(filepath, title, poem, poem_style)
                    print(f"Auto-print: Generated poem '{title}' ({poem_style}) and queued for printing. Total poems: {poem_count}")
                    
                    # Clear generation status
                    poem_generation_status["is_generating"] = False
                    poem_generation_status["filename"] = None
                    poem_generation_status["started_at"] = None
                    
                except Exception as e:
                    print(f"Auto-print failed: {str(e)}")
                    # Clear generation status on error
                    poem_generation_status["is_generating"] = False
                    poem_generation_status["filename"] = None
                    poem_generation_status["started_at"] = None
            
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
            # Also delete associated poems file
            poems_json_path = os.path.splitext(filepath)[0] + '_poems.json'
            if os.path.exists(poems_json_path):
                os.remove(poems_json_path)
            return jsonify({'success': True, 'message': 'Image deleted'})
        return jsonify({'error': 'Image not found'}), 404
    
    @app.route('/api/delete-poem/<filename>/<poem_id>', methods=['POST'])
    def delete_poem(filename, poem_id):
        """Delete a specific poem from an image"""
        try:
            upload_folder = app.config['UPLOAD_FOLDER']
            filepath = os.path.join(upload_folder, filename)
            
            if not os.path.exists(filepath):
                return jsonify({'error': 'Image not found'}), 404
            
            poems_json_path = os.path.splitext(filepath)[0] + '_poems.json'
            
            if not os.path.exists(poems_json_path):
                return jsonify({'error': 'No poems found for this image'}), 404
            
            # Load poems
            with open(poems_json_path, 'r') as f:
                poems_list = json.load(f)
            
            # Filter out the poem with matching ID
            updated_poems = [p for p in poems_list if p.get('id') != poem_id]
            
            if len(updated_poems) == len(poems_list):
                return jsonify({'error': 'Poem not found'}), 404
            
            # Save updated poems list or delete file if empty
            if updated_poems:
                with open(poems_json_path, 'w') as f:
                    json.dump(updated_poems, f, indent=2)
            else:
                os.remove(poems_json_path)
            
            return jsonify({
                'success': True,
                'message': 'Poem deleted',
                'remaining_count': len(updated_poems)
            })
            
        except Exception as e:
            return jsonify({'error': f'Failed to delete poem: {str(e)}'}), 500
    
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
            custom_context = data.get('context', '')
            
            # Generate the poem using the helper function
            title, poem = generate_poem_for_image(filepath, poet_style, location, custom_context)
            
            # Save the poem using the helper function
            poem_count = save_poem_to_file(filepath, title, poem, poet_style)
            print(f"Generated poem '{title}' ({poet_style}) for {filename}. Total poems: {poem_count}")

            return jsonify({
                'success': True,
                'poem': poem,
                'title': title,
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
    def get_poet_prompts(location_context="", additional_context=""):
        """Get poet-specific prompts with optional context"""
        return {
            'general': f"""Analyze this image and create a beautiful, creative poem about it. 
            The poem should:
            - Be 8-12 lines long
            - Capture the essence, mood, and visual elements of the image
            - Use vivid imagery and poetic language
            - Have a flowing rhythm
            - Be emotionally evocative{location_context}{additional_context}
            
            Format your response EXACTLY as follows:
            TITLE: [A short, evocative title for the poem, 2-5 words]
            
            [The poem text]
            
            Example format:
            TITLE: Autumn's Gentle Whisper
            
            Golden leaves dance down
            Through the crisp morning air...""",
            
            'shakespeare': f"""Analyze this image and create a poem in the style of William Shakespeare.
            - Use Shakespearean language and imagery
            - Include metaphors and dramatic flair
            - Use iambic pentameter if possible
            - Be 8-12 lines long
            - Capture the dramatic essence of the image{location_context}{additional_context}
            
            Format your response EXACTLY as follows:
            TITLE: [A short, evocative title for the poem, 2-5 words]
            
            [The poem text]""",
            
            'dickinson': f"""Analyze this image and create a poem in the style of Emily Dickinson.
            - Use short, concise lines with dashes
            - Focus on nature, death, immortality, or inner emotions
            - Include slant rhyme and unconventional capitalization
            - Be introspective and contemplative
            - Be 8-12 lines long{location_context}{additional_context}
            
            Format your response EXACTLY as follows:
            TITLE: [A short, evocative title for the poem, 2-5 words]
            
            [The poem text]""",
            
            'frost': f"""Analyze this image and create a poem in the style of Robert Frost.
            - Use conversational yet profound language
            - Include natural imagery and rural scenes
            - Have a narrative quality with deeper meaning
            - Use clear, accessible language with hidden depths
            - Be 8-12 lines long{location_context}{additional_context}
            
            Format your response EXACTLY as follows:
            TITLE: [A short, evocative title for the poem, 2-5 words]
            
            [The poem text]""",
            
            'angelou': f"""Analyze this image and create a poem in the style of Maya Angelou.
            - Use powerful, rhythmic language
            - Include themes of resilience, strength, and humanity
            - Be empowering and uplifting
            - Use vivid, sensory details
            - Be 8-12 lines long{location_context}{additional_context}
            
            Format your response EXACTLY as follows:
            TITLE: [A short, evocative title for the poem, 2-5 words]
            
            [The poem text]""",
            
            'poe': f"""Analyze this image and create a poem in the style of Edgar Allan Poe.
            - Use dark, gothic, and mysterious imagery
            - Include melancholy and haunting themes
            - Use musical, rhythmic language
            - Create an atmosphere of suspense or sorrow
            - Be 8-12 lines long{location_context}{additional_context}
            
            Format your response EXACTLY as follows:
            TITLE: [A short, evocative title for the poem, 2-5 words]
            
            [The poem text]""",
            
            'whitman': f"""Analyze this image and create a poem in the style of Walt Whitman.
            - Use free verse with long, flowing lines
            - Celebrate life, nature, and humanity
            - Include expansive, all-embracing imagery
            - Be bold and declarative
            - Be 8-12 lines long{location_context}{additional_context}
            
            Format your response EXACTLY as follows:
            TITLE: [A short, evocative title for the poem, 2-5 words]
            
            [The poem text]""",
            
            'haiku': f"""Analyze this image and create a traditional haiku.
            - Follow the 5-7-5 syllable pattern exactly
            - Focus on nature, seasons, or a moment in time
            - Include a subtle reference to the natural world
            - Capture a fleeting moment or emotion
            - Be exactly 3 lines{location_context}{additional_context}
            
            Format your response EXACTLY as follows:
            TITLE: [A short, evocative title for the poem, 2-5 words]
            
            [The haiku text]""",
            
            'free_verse': f"""Analyze this image and create a free verse poem.
            - Use varied line lengths and natural speech rhythms
            - No rhyme scheme or fixed meter required
            - Include visual white space and intentional line breaks
            - Use enjambment and strategic pauses
            - Vary indentation and alignment for visual effect
            - Be 8-14 lines long
            - Focus on authentic, raw emotion and imagery
            - Let the form follow the content naturally{location_context}{additional_context}
            
            Format your response EXACTLY as follows:
            TITLE: [A short, evocative title for the poem, 2-5 words]
            
            [The poem text with varied alignment and spacing]"""
        }
    
    def generate_poem_for_image(filepath, poet_style='general', location=None, custom_context=''):
        """
        Generate a poem for an image using Gemini API
        Returns: (title, poem_text) tuple or raises exception
        """
        if not GEMINI_API_KEY:
            raise ValueError('Gemini API key not configured')
        
        # Build location context if available
        location_context = ""
        if location:
            lat = location.get('latitude')
            lon = location.get('longitude')
            if lat and lon:
                location_context = f"\n\nThe viewer is located at approximately latitude {lat:.2f}, longitude {lon:.2f}. Consider incorporating elements of this geographic location, climate, or regional characteristics subtly into the poem if relevant."
        
        # Build custom context if provided
        additional_context = ""
        if custom_context:
            additional_context = f"\n\nAdditional context from the user: {custom_context}\nPlease incorporate this context, mood, or theme into the poem."
        
        # Get prompts
        poet_prompts = get_poet_prompts(location_context, additional_context)
        
        # Open the image
        img = Image.open(filepath)
        
        # Initialize Gemini model
        model = genai.GenerativeModel('gemini-2.5-flash')
        
        # Get the appropriate prompt
        prompt = poet_prompts.get(poet_style, poet_prompts['general'])
        
        response = model.generate_content([prompt, img])
        poem_response = response.text.strip()
        
        # Parse the title and poem from the response
        title = "Untitled"
        poem = poem_response
        
        # Look for "TITLE: " format (case insensitive)
        lines = poem_response.split('\n')
        title_found = False
        poem_lines = []
        
        for i, line in enumerate(lines):
            line_stripped = line.strip()
            if not title_found and line_stripped.upper().startswith("TITLE:"):
                # Extract title (remove "TITLE: " prefix)
                title = line_stripped[6:].strip()  # Remove "TITLE:" (6 chars)
                title_found = True
            elif title_found and line_stripped:  # Skip empty lines after title
                # This is poem content - preserve leading whitespace for indentation
                poem_lines.append(line.rstrip())  # Only strip trailing whitespace
            elif title_found and not line_stripped and poem_lines:
                # Empty line within poem, keep it
                poem_lines.append('')
        
        # If we found a title, use the parsed poem lines, otherwise use original
        if title_found and poem_lines:
            poem = '\n'.join(poem_lines)
        
        return title, poem
    
    def save_poem_to_file(filepath, title, poem_text, poet_style):
        """
        Save poem to _poems.json file next to the image
        Handles multiple poems per image and style deduplication
        """
        import uuid
        
        poems_json_path = os.path.splitext(filepath)[0] + '_poems.json'
        
        # Load existing poems if file exists
        poems_list = []
        if os.path.exists(poems_json_path):
            try:
                with open(poems_json_path, 'r') as f:
                    poems_list = json.load(f)
            except:
                poems_list = []
        
        # Create new poem object with unique ID
        new_poem = {
            'id': str(uuid.uuid4()),
            'title': title,
            'poet_style': poet_style.capitalize(),
            'poem_text': poem_text,
            'generated_at': datetime.now().isoformat()
        }
        
        # Check if this style already exists and replace it
        style_exists = False
        for i, existing_poem in enumerate(poems_list):
            if existing_poem.get('poet_style', '').lower() == poet_style.lower():
                poems_list[i] = new_poem
                style_exists = True
                break
        
        # If style doesn't exist, append new poem
        if not style_exists:
            poems_list.append(new_poem)
        
        # Save updated poems list
        with open(poems_json_path, 'w') as f:
            json.dump(poems_list, f, indent=2)
        
        return len(poems_list)
    
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
                
                # Check for poems - support both old _poem.json and new _poems.json
                poems_json_path = os.path.splitext(filepath)[0] + '_poems.json'
                old_poem_json_path = os.path.splitext(filepath)[0] + '_poem.json'
                poems_data = []
                
                if os.path.exists(poems_json_path):
                    # New format: array of poems
                    try:
                        with open(poems_json_path, 'r') as f:
                            poems_data = json.load(f)
                    except Exception as e:
                        print(f"Error loading poems for {filename}: {e}")
                elif os.path.exists(old_poem_json_path):
                    # Old format: single poem object - migrate it
                    try:
                        with open(old_poem_json_path, 'r') as f:
                            old_poem = json.load(f)
                        # Convert to array format
                        import uuid
                        old_poem['id'] = str(uuid.uuid4())
                        poems_data = [old_poem]
                        # Save in new format and delete old file
                        with open(poems_json_path, 'w') as f:
                            json.dump(poems_data, f, indent=2)
                        os.remove(old_poem_json_path)
                        print(f"Migrated old poem format for {filename}")
                    except Exception as e:
                        print(f"Error migrating old poem for {filename}: {e}")
                
                image_info = {
                    'filename': filename,
                    'size': stat.st_size,
                    'modified': datetime.fromtimestamp(stat.st_mtime).isoformat(),
                    'url': f'/images/{filename}'
                }
                
                if poems_data:
                    image_info['poems'] = poems_data
                    image_info['poem_count'] = len(poems_data)
                
                images.append(image_info)
        
        # Sort by modified date, newest first
        images.sort(key=lambda x: x['modified'], reverse=True)
        return images
    
    @app.route('/api/ota-update', methods=['POST'])
    def trigger_ota_update():
        """Trigger OTA update on ESP32"""
        global command_queue
        
        # Queue the OTA update command
        command_queue["command"] = "ota_update"
        
        return jsonify({
            'success': True,
            'message': 'OTA update triggered',
            'update_available': True,
            'current_version': 'unknown',
            'latest_version': 'checking...'
        })
    
    return app
