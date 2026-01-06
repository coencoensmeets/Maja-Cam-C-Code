#!/usr/bin/env python3
"""
Main entry point for the Poem Camera Flask Server
Can run locally or on PythonAnywhere
"""
import os
from dotenv import load_dotenv

# Load environment variables from .env file
load_dotenv()

from app import create_app

# Create the Flask application
app = create_app()

if __name__ == '__main__':
    # Check if running on PythonAnywhere
    is_pythonanywhere = os.environ.get('PYTHONANYWHERE_SITE') is not None
    
    if is_pythonanywhere:
        # PythonAnywhere uses WSGI, so this won't actually run
        # but we keep it for compatibility
        app.run()
    else:
        # Local development server
        port = int(os.environ.get('PORT', 5000))
        debug = os.environ.get('FLASK_ENV') == 'development'
        
        print(f"\n{'='*60}")
        print(f"  Poem Camera Server Starting")
        print(f"{'='*60}")
        print(f"  Environment: {'Development' if debug else 'Production'}")
        print(f"  Port: {port}")
        print(f"  URL: http://127.0.0.1:{port}")
        print(f"{'='*60}\n")
        
        # Try to use LiveReload for automatic browser refresh on file changes
        try:
            from livereload import Server
            use_livereload = True
        except Exception:
            use_livereload = False

        if use_livereload and debug:
            base_dir = os.path.dirname(__file__)
            server = Server(app.wsgi_app)
            # Watch templates and static assets in this website package
            server.watch(os.path.join(base_dir, 'templates'))
            server.watch(os.path.join(base_dir, 'static'))
            # Watch python files in this package so code changes trigger reload
            server.watch(os.path.join(base_dir, '*.py'))
            server.watch(os.path.join(base_dir, 'app.py'))

            print('Using LiveReload development server (auto-refresh enabled)')
            server.serve(host='0.0.0.0', port=port, debug=debug)
        else:
            app.run(
                host='0.0.0.0',
                port=port,
                debug=debug
            )
