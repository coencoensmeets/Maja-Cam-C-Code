# WSGI Configuration for PythonAnywhere
# 
# This file tells PythonAnywhere how to run your Flask application
# Update the path below to match your PythonAnywhere directory structure

import sys
import os

# Add your project directory to the sys.path
# CHANGE THIS to match your PythonAnywhere directory
project_home = '/home/yourusername/flask_server'
if project_home not in sys.path:
    sys.path.insert(0, project_home)

# Set environment variables
os.environ['FLASK_ENV'] = 'production'

# Import the Flask app
from app import create_app
application = create_app()
