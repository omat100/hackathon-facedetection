# Backend Integration Guide

This document explains how to connect the React Native frontend with the Python backend for face recognition and attendance tracking.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│           React Native Mobile App                           │
│  (Face Attendance - Android/iOS)                            │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       │ HTTP REST API
                       │ (JSON over HTTP)
                       ▼
┌─────────────────────────────────────────────────────────────┐
│         Flask API Server (api_server.py)                    │
│    - Handles image upload and processing                    │
│    - Bridges frontend and backend services                  │
│    - Manages synchronization                                │
└──────────────────┬─────────────────────┬────────────────────┘
                   │                     │
        ┌──────────▼──────────┐  ┌──────▼──────────────────┐
        │  Face Recognition   │  │  Liveness Detection    │
        │  Engine (Python)    │  │  (MediaPipe/OpenCV)    │
        │  - InsightFace      │  │  - Face Landmarks      │
        │  - Deep Learning    │  │  - Head Movements      │
        └─────────────────────┘  └───────────────────────┘
```

## Setup Instructions

### 1. Install Backend Dependencies

```bash
cd hackathon-facedetection

# Create virtual environment (recommended)
python -m venv venv

# Activate virtual environment
# On Windows:
venv\Scripts\activate
# On macOS/Linux:
source venv/bin/activate

# Install required packages
pip install flask flask-cors werkzeug
pip install insightface opencv-python mediapipe numpy
pip install sqlite3  # Usually built-in
```

### 2. Start the API Server

```bash
# From the hackathon-facedetection directory
python api_server.py
```

Expected output:
```
╔═════════════════════════════════════════════╗
║   Face Attendance API Server v1.0.0         ║
║   Running on http://localhost:5000          ║
╚═════════════════════════════════════════════╝
```

### 3. Configure Frontend Connection

Edit `frontend/AttendanceApp/src/services/FaceRecognitionService.ts`:

```typescript
// Change this line to your backend URL
private backendUrl: string = 'http://localhost:5000';

// For production, use your server IP/domain
// private backendUrl: string = 'http://192.168.x.x:5000';
```

Similarly in `src/services/SyncService.ts`:

```typescript
private config: SyncConfig = {
  backendUrl: 'http://localhost:5000',  // Update this
  batchSize: 50,
  retryAttempts: 3,
};
```

### 4. Test Connection

Run a health check:
```bash
curl http://localhost:5000/
```

Expected response:
```json
{
  "status": "success",
  "message": "Face Attendance API Server is running",
  "version": "1.0.0",
  "timestamp": "2024-06-01T10:30:00.000000"
}
```

## API Endpoints Documentation

### 1. Health Check
```
GET /
Response: {status, message, version, timestamp}
```

### 2. Face Recognition
```
POST /recognize
Content-Type: multipart/form-data
Body: {frame: <image_file>}

Response: {
  "status": "success",
  "data": {
    "person_id": "emp001",
    "person_name": "John Doe",
    "confidence": 0.95,
    "liveness_verified": true
  }
}
```

### 3. Liveness Detection
```
POST /liveness-check
Content-Type: multipart/form-data
Body: {frame: <image_file>}

Response: {
  "status": "success",
  "verified": true,
  "confidence": 0.92
}
```

### 4. Sync Attendance
```
POST /sync-attendance
Content-Type: application/json
Body: {
  "records": [
    {
      "person_id": "emp001",
      "timestamp": "2024-06-01T10:30:00Z",
      "confidence": 0.95
    }
  ],
  "timestamp": "2024-06-01T10:30:00Z",
  "client_info": {
    "platform": "react-native",
    "version": "1.0.0"
  }
}

Response: {
  "status": "success",
  "message": "Synced X records",
  "synced_count": 5,
  "total_records": 5
}
```

### 5. Get Persons
```
GET /persons

Response: {
  "status": "success",
  "persons": [
    {"id": "emp001", "name": "John Doe"},
    {"id": "emp002", "name": "Jane Smith"}
  ],
  "count": 2
}
```

### 6. Get Statistics
```
GET /attendance-stats

Response: {
  "status": "success",
  "stats": {
    "total": 100,
    "synced": 95,
    "unsynced": 5
  }
}
```

### 7. Get Configuration
```
GET /config

Response: {
  "status": "success",
  "config": {
    "max_file_size": 5242880,
    "allowed_extensions": ["png", "jpg", "jpeg", "gif", "webp"],
    "batch_size": 50,
    "sync_enabled": true
  }
}
```

## Network Configuration

### For Local Development

**If running on same machine:**
```
Backend: http://localhost:5000
Frontend: Expo Go or emulator
```

**If running on different machines:**

1. Find your machine's IP:
```bash
# Windows
ipconfig | findstr IPv4

# macOS/Linux
ifconfig | grep inet
```

2. Update frontend config:
```typescript
private backendUrl: string = 'http://192.168.x.x:5000';
```

3. Ensure firewall allows port 5000:
```bash
# Windows
netsh advfirewall firewall add rule name="Flask API" dir=in action=allow protocol=tcp localport=5000
```

### For Production

1. Use proper domain/IP address
2. Enable HTTPS:
```python
app.run(ssl_context='adhoc')  # or use SSL certificate
```

3. Add authentication:
```python
@app.before_request
def require_auth():
    auth = request.headers.get('Authorization')
    # Validate token
```

4. Implement rate limiting:
```python
from flask_limiter import Limiter
limiter = Limiter(app, key_func=lambda: request.remote_addr)
```

## File Upload Configuration

### Image Size Limits
- Current limit: 5MB
- Supported formats: PNG, JPG, JPEG, GIF, WebP
- Recommended: 480x640 or higher resolution

To change limits, edit `api_server.py`:
```python
MAX_FILE_SIZE = 10 * 1024 * 1024  # 10MB
ALLOWED_EXTENSIONS = {'png', 'jpg', 'jpeg', 'gif', 'webp'}
```

## Database Synchronization

### Sync Flow

1. **Local Recording** (Offline)
   - App records attendance to local SQLite
   - Records marked with `synced: false`

2. **Network Detection**
   - App detects internet connection
   - Checks for pending records

3. **Batch Sync**
   - Groups records into batches (default: 50)
   - Sends to `/sync-attendance` endpoint

4. **Confirmation**
   - Backend confirms receipt
   - App marks records as `synced: true`

5. **Retry Logic**
   - Failed records retry (configurable attempts)
   - Exponential backoff prevents server overload

### Manual Sync

User can trigger manual sync with "🔄 Sync" button in app.

## TensorFlow Lite Integration

To integrate TFLite models for on-device ML:

### 1. Prepare Model

```bash
# Convert Python model to TFLite format
python model_conversion.py --input model.h5 --output model.tflite
```

### 2. Add to Frontend

```typescript
import { TFLiteInterpreter } from 'react-native-fast-tflite';

const interpreter = await TFLiteInterpreter.fromAsset('models/model.tflite');
const outputs = await interpreter.runInference([input]);
```

### 3. Update Face Recognition Service

```typescript
private async recognizeFaceWithTFLite(imageData: any) {
  const outputs = await this.interpreter.runInference([imageData]);
  // Process outputs
}
```

## Troubleshooting

### Connection Issues

**Problem:** Frontend can't connect to backend
```
Solution:
1. Check backend is running: curl http://localhost:5000
2. Verify IP address and port
3. Check firewall settings
4. Look at backend console for errors
```

### File Upload Issues

**Problem:** "File too large" error
```
Solution:
1. Reduce image size
2. Increase MAX_FILE_SIZE in api_server.py
3. Check available disk space
```

### Sync Failures

**Problem:** Records not syncing
```
Solution:
1. Check network connectivity
2. Verify backend is accessible
3. Check /sync-attendance response
4. Look for errors in app logs
```

### Face Recognition Not Working

**Problem:** Recognition always fails
```
Solution:
1. Ensure proper lighting
2. Clear camera lens
3. Check backend pipeline is initialized
4. Verify InsightFace model files exist
```

## Performance Optimization

### Backend

```python
# Use connection pooling
SQLALCHEMY_POOL_SIZE = 10
SQLALCHEMY_POOL_RECYCLE = 3600

# Cache person database
@cache.cached(timeout=300)
def get_persons():
    pass

# Implement pagination
@app.route('/attendance-records?page=1&limit=20')
```

### Frontend

```typescript
// Debounce search
const [searchText] = useDebounce(text, 300);

// Batch sync requests
const BATCH_SIZE = 50;

// Lazy load records
useEffect(() => {
  loadMore();
}, []);
```

## Security Best Practices

1. **API Authentication**
```python
from functools import wraps

def require_token(f):
    @wraps(f)
    def decorated(*args, **kwargs):
        token = request.headers.get('Authorization')
        if not verify_token(token):
            return jsonify({'status': 'failed'}), 401
        return f(*args, **kwargs)
    return decorated
```

2. **Input Validation**
```python
from werkzeug.utils import secure_filename

filename = secure_filename(file.filename)
```

3. **CORS Configuration**
```python
CORS(app, resources={
    r"/api/*": {
        "origins": ["http://localhost:19000"],
        "methods": ["POST", "GET", "OPTIONS"],
        "allow_headers": ["Content-Type", "Authorization"]
    }
})
```

4. **Rate Limiting**
```python
from flask_limiter import Limiter
limiter = Limiter(
    app,
    key_func=lambda: request.remote_addr,
    default_limits=["200 per day", "50 per hour"]
)
```

## Monitoring and Logging

### Enable Logging

```python
import logging

logging.basicConfig(
    filename='app.log',
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)
```

### Health Monitoring

```python
from flask import g
import time

@app.before_request
def before_request():
    g.start_time = time.time()

@app.after_request
def after_request(response):
    elapsed = time.time() - g.start_time
    logger.info(f"{request.path} - {elapsed}s")
    return response
```

## Docker Deployment

Create `Dockerfile`:

```dockerfile
FROM python:3.9-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY . .

CMD ["python", "api_server.py"]
```

Build and run:
```bash
docker build -t face-attendance-api .
docker run -p 5000:5000 face-attendance-api
```

## Additional Resources

- [InsightFace Documentation](https://insightface.ai/)
- [Flask Documentation](https://flask.palletsprojects.com/)
- [React Native Expo Docs](https://docs.expo.dev/)
- [SQLite Documentation](https://www.sqlite.org/docs.html)
- [MediaPipe Face Detection](https://google.github.io/mediapipe/)

## Support

For issues or questions:
1. Check error logs in console
2. Review API response status
3. Verify backend state
4. Check network connectivity
5. Contact development team
