"""
Flask API Server for Face Attendance App
Bridges React Native frontend with Python backend services
"""

import os
import json
import sys
from datetime import datetime
from flask import Flask, request, jsonify
from flask_cors import CORS
from werkzeug.utils import secure_filename

# Add parent directory to path to import backend modules
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    from main_pipeline import AttendancePipeline
    from storage import AttendanceStorage
    from face_recognition_engine import FaceRecognitionEngine
    from liveness_detection import LivenessDetector
except ImportError as e:
    print(f"Warning: Could not import backend modules: {e}")
    print("Running in mock mode...")

app = Flask(__name__)
CORS(app)

# Configuration
UPLOAD_FOLDER = 'uploads'
ALLOWED_EXTENSIONS = {'png', 'jpg', 'jpeg', 'gif', 'webp'}
MAX_FILE_SIZE = 5 * 1024 * 1024  # 5MB

if not os.path.exists(UPLOAD_FOLDER):
    os.makedirs(UPLOAD_FOLDER)

app.config['MAX_CONTENT_LENGTH'] = MAX_FILE_SIZE
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER

# Initialize services
try:
    pipeline = AttendancePipeline()
    storage = AttendanceStorage()
except Exception as e:
    print(f"Warning: Could not initialize pipeline: {e}")
    pipeline = None
    storage = None

# Mock database for demo mode
MOCK_EMPLOYEES = {
    'emp001': 'John Doe',
    'emp002': 'Jane Smith',
    'emp003': 'Mike Johnson',
    'emp004': 'Sarah Williams',
    'emp005': 'Om Thakur',
    'emp006': 'Priya Singh',
    'emp007': 'Raj Patel',
}


def allowed_file(filename):
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS


# API Endpoints

@app.route('/', methods=['GET'])
def health_check():
    """Health check endpoint"""
    return jsonify({
        'status': 'success',
        'message': 'Face Attendance API Server is running',
        'version': '1.0.0',
        'timestamp': datetime.now().isoformat(),
    })


@app.route('/recognize', methods=['POST'])
def recognize_face():
    """
    Face recognition endpoint
    Expects: image file in multipart/form-data
    Returns: person_id, person_name, confidence, liveness_verified
    """
    try:
        # Check if file is in request
        if 'frame' not in request.files:
            return jsonify({
                'status': 'failed',
                'message': 'No frame provided'
            }), 400

        file = request.files['frame']

        if file.filename == '':
            return jsonify({
                'status': 'failed',
                'message': 'No file selected'
            }), 400

        if not allowed_file(file.filename):
            return jsonify({
                'status': 'failed',
                'message': 'Invalid file format'
            }), 400

        # Save file temporarily
        filename = secure_filename(f"{datetime.now().timestamp()}_{file.filename}")
        filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
        file.save(filepath)

        try:
            # Process with pipeline if available
            if pipeline:
                # Implementation depends on your pipeline
                # This is a placeholder
                result = {
                    'person_id': 'emp001',
                    'person_name': 'John Doe',
                    'confidence': 0.95,
                    'liveness_verified': True,
                }
            else:
                # Mock result for demo
                import random
                emp_id = f'emp{random.randint(1, 7):03d}'
                result = {
                    'person_id': emp_id,
                    'person_name': MOCK_EMPLOYEES.get(emp_id, 'Unknown'),
                    'confidence': random.uniform(0.80, 0.99),
                    'liveness_verified': random.random() > 0.2,
                }

            return jsonify({
                'status': 'success',
                'data': result
            })

        finally:
            # Clean up temp file
            if os.path.exists(filepath):
                os.remove(filepath)

    except Exception as e:
        print(f"Error in /recognize: {e}")
        return jsonify({
            'status': 'failed',
            'message': str(e)
        }), 500


@app.route('/liveness-check', methods=['POST'])
def liveness_check():
    """
    Liveness detection endpoint
    Expects: image frame in multipart/form-data
    Returns: verified, confidence
    """
    try:
        if 'frame' not in request.files:
            return jsonify({
                'status': 'failed',
                'message': 'No frame provided'
            }), 400

        file = request.files['frame']

        if not allowed_file(file.filename):
            return jsonify({
                'status': 'failed',
                'message': 'Invalid file format'
            }), 400

        # For demo, return mock result
        import random
        verified = random.random() > 0.3
        confidence = random.uniform(0.6, 0.99) if verified else random.uniform(0.2, 0.5)

        return jsonify({
            'status': 'success',
            'verified': verified,
            'confidence': float(confidence)
        })

    except Exception as e:
        print(f"Error in /liveness-check: {e}")
        return jsonify({
            'status': 'failed',
            'message': str(e)
        }), 500


@app.route('/sync-attendance', methods=['POST'])
def sync_attendance():
    """
    Sync attendance records from mobile app to server
    Expects: JSON with attendance records array
    """
    try:
        data = request.get_json()

        if not data or 'records' not in data:
            return jsonify({
                'status': 'failed',
                'message': 'No records provided'
            }), 400

        records = data['records']
        synced_count = 0

        # Process each record
        for record in records:
            try:
                person_id = record.get('person_id')
                confidence = record.get('confidence')
                timestamp = record.get('timestamp')

                if not person_id:
                    continue

                # Store in backend if storage is available
                if storage:
                    storage.log(person_id, confidence)

                synced_count += 1

            except Exception as e:
                print(f"Error processing record: {e}")
                continue

        return jsonify({
            'status': 'success',
            'message': f'Synced {synced_count} records',
            'synced_count': synced_count,
            'total_records': len(records)
        })

    except Exception as e:
        print(f"Error in /sync-attendance: {e}")
        return jsonify({
            'status': 'failed',
            'message': str(e)
        }), 500


@app.route('/persons', methods=['GET'])
def get_persons():
    """
    Get list of all persons/employees
    Returns: array of {id, name} objects
    """
    try:
        persons = [
            {'id': emp_id, 'name': name}
            for emp_id, name in MOCK_EMPLOYEES.items()
        ]

        return jsonify({
            'status': 'success',
            'persons': persons,
            'count': len(persons)
        })

    except Exception as e:
        print(f"Error in /persons: {e}")
        return jsonify({
            'status': 'failed',
            'message': str(e)
        }), 500


@app.route('/attendance-stats', methods=['GET'])
def get_stats():
    """
    Get attendance statistics
    """
    try:
        if storage:
            stats = storage.stats()
        else:
            stats = {'total': 0, 'synced': 0, 'unsynced': 0}

        return jsonify({
            'status': 'success',
            'stats': stats
        })

    except Exception as e:
        print(f"Error in /attendance-stats: {e}")
        return jsonify({
            'status': 'failed',
            'message': str(e)
        }), 500


@app.route('/attendance-records', methods=['GET'])
def get_attendance_records():
    """
    Get attendance records from server
    Query params: limit (default: 100)
    """
    try:
        limit = request.args.get('limit', 100, type=int)

        if storage:
            records = storage.get_unsynced()
        else:
            records = []

        return jsonify({
            'status': 'success',
            'records': records,
            'count': len(records)
        })

    except Exception as e:
        print(f"Error in /attendance-records: {e}")
        return jsonify({
            'status': 'failed',
            'message': str(e)
        }), 500


@app.route('/config', methods=['GET'])
def get_config():
    """
    Get server configuration
    """
    return jsonify({
        'status': 'success',
        'config': {
            'max_file_size': MAX_FILE_SIZE,
            'allowed_extensions': list(ALLOWED_EXTENSIONS),
            'batch_size': 50,
            'sync_enabled': True,
        }
    })


@app.errorhandler(413)
def request_entity_too_large(error):
    """Handle file too large error"""
    return jsonify({
        'status': 'failed',
        'message': 'File too large. Maximum size is 5MB.'
    }), 413


@app.errorhandler(404)
def not_found(error):
    """Handle 404 errors"""
    return jsonify({
        'status': 'failed',
        'message': 'Endpoint not found'
    }), 404


@app.errorhandler(500)
def internal_error(error):
    """Handle 500 errors"""
    return jsonify({
        'status': 'failed',
        'message': 'Internal server error'
    }), 500


if __name__ == '__main__':
    print("""
    ╔═════════════════════════════════════════════╗
    ║   Face Attendance API Server v1.0.0         ║
    ║   Running on http://localhost:5000          ║
    ╚═════════════════════════════════════════════╝
    """)
    app.run(host='0.0.0.0', port=5000, debug=True)
