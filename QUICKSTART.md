# Quick Start Guide

This guide will help you get the entire Face Attendance system up and running.

## Prerequisites

- **Python 3.8+** - For backend
- **Node.js 16+** - For React Native/Expo
- **npm** - Package manager
- **Git** - Version control
- Android Studio or XCode (optional, for native builds)

## Step-by-Step Setup

### 1. Backend Setup (Python)

```bash
# Navigate to project directory
cd hackathon-facedetection

# Create virtual environment
python -m venv venv

# Activate virtual environment
# Windows:
venv\Scripts\activate
# macOS/Linux:
source venv/bin/activate

# Install dependencies
pip install -r requirements.txt

# Verify installation
python -c "import cv2, insightface, mediapipe; print('✓ All dependencies installed')"
```

### 2. Frontend Setup (React Native)

```bash
# Navigate to frontend directory
cd frontend/AttendanceApp

# Install dependencies
npm install

# Verify Expo is installed
npx expo --version
```

### 3. Start the Backend API Server

**Terminal 1:**

```bash
cd hackathon-facedetection

# Activate virtual environment (if not already active)
source venv/bin/activate  # or venv\Scripts\activate on Windows

# Start API server
python api_server.py
```

You should see:
```
╔═════════════════════════════════════════════╗
║   Face Attendance API Server v1.0.0         ║
║   Running on http://localhost:5000          ║
╚═════════════════════════════════════════════╝
```

### 4. Start the Frontend Development Server

**Terminal 2:**

```bash
cd frontend/AttendanceApp

# Start Expo development server
npm start
```

You should see:
```
▶  Metro waiting on exp://192.168.x.x:19000
```

### 5. Run on Device/Emulator

**Option A: Expo Go (Mobile Device)**
- Install "Expo Go" app on your phone
- Scan the QR code from Terminal 2
- App opens on your phone

**Option B: Android Emulator**
```bash
# From Terminal 2
npm run android
```

**Option C: iOS Simulator (macOS only)**
```bash
# From Terminal 2
npm run ios
```

**Option D: Web Browser**
```bash
# From Terminal 2
npm run web
# Opens at http://localhost:19006
```

### 6. Test the Connection

1. Open the app
2. You should see:
   - ✓ Green "🌐 Online" status (if backend is running)
   - List of attendance records (if any)
   - Search bar and "Mark Attendance" button

3. Test face recognition:
   - Tap "Mark Attendance"
   - Follow the camera prompts
   - See success screen

## Troubleshooting

### Backend Issues

**Port 5000 already in use:**
```bash
# Find process using port 5000
# Windows:
netstat -ano | findstr :5000

# Kill the process
taskkill /PID <PID> /F

# Or use different port in api_server.py:
app.run(port=5001)
```

**Module not found errors:**
```bash
# Reinstall packages
pip install -r requirements.txt --force-reinstall

# Check Python version
python --version  # Should be 3.8+
```

**Model download issues:**
```bash
# Download models manually if needed
python
>>> import insightface
>>> app = insightface.app.FaceAnalysis()
>>> app.prepare(ctx_id=0)
```

### Frontend Issues

**npm dependency issues:**
```bash
# Clear cache and reinstall
rm -rf node_modules package-lock.json
npm install
```

**Expo Go doesn't connect:**
```bash
# Check if on same WiFi network
# If using emulator, use:
# http://127.0.0.1:5000 instead of localhost

# Update backend URL in:
# src/services/FaceRecognitionService.ts
# src/services/SyncService.ts
```

**Camera permission denied:**
- Android: Settings > Apps > Face Attendance > Permissions > Camera
- iOS: Settings > Face Attendance > Camera

## Project Structure

```
hackathon-facedetection/
├── api_server.py                 # Flask API server
├── main_pipeline.py              # Attendance pipeline
├── face_recognition_engine.py    # Face detection logic
├── liveness_detection.py         # Liveness checks
├── storage.py                    # SQLite database
├── requirements.txt              # Python dependencies
├── attendance.db                 # Local SQLite database
├── BACKEND_INTEGRATION.md        # Detailed integration guide
├── cpp/
│   └── liveness_engine/          # C++ optimization (optional)
├── preprocessing/
│   └── preprocessing.py
├── training/
│   └── train.py
└── frontend/
    ├── README.md                 # Frontend documentation
    └── AttendanceApp/
        ├── src/
        │   ├── screens/          # UI screens
        │   ├── components/       # Reusable components
        │   ├── services/         # Business logic
        │   └── types/            # TypeScript definitions
        ├── App.tsx               # Main app file
        ├── app.json              # Expo configuration
        └── package.json          # NPM dependencies
```

## Development Workflow

1. **Make backend changes:**
   - Edit Python files
   - Restart api_server.py (Ctrl+C, then run again)
   - Changes take effect immediately

2. **Make frontend changes:**
   - Edit React components
   - Save file
   - Hot reload happens automatically in Expo
   - Or press 'r' in Expo terminal to refresh

3. **Test integration:**
   - Check backend health: `curl http://localhost:5000`
   - Test API endpoints using Postman or curl
   - Test app features on device/emulator

## Building for Production

### Android

```bash
cd frontend/AttendanceApp
npm install -g eas-cli
eas build --platform android --auto
```

### iOS

```bash
cd frontend/AttendanceApp
npm install -g eas-cli
eas build --platform ios --auto
```

### Web

```bash
cd frontend/AttendanceApp
npm run build:web
npm install -g serve
serve dist -p 3000
```

## Performance Tips

1. **Backend:**
   - Use production Flask server (Gunicorn)
   - Enable caching for persons database
   - Implement connection pooling
   - Use async image processing

2. **Frontend:**
   - Enable React Native Hermes
   - Optimize image compression
   - Implement pagination for records
   - Use lazy loading

3. **Database:**
   - Index frequently queried columns
   - Regular database cleanup
   - Archive old records

## Security Checklist

- [ ] Change default backend URL from localhost
- [ ] Add API authentication tokens
- [ ] Enable HTTPS for production
- [ ] Implement rate limiting
- [ ] Add input validation
- [ ] Secure sensitive data
- [ ] Implement proper logging
- [ ] Regular security audits

## Next Steps

1. **Integrate with real ML models:**
   - Replace mock face recognition with actual pipeline
   - Connect liveness detection properly

2. **Add database models:**
   - Employee information management
   - Attendance reports
   - Performance analytics

3. **Implement advanced features:**
   - Multi-face detection
   - Real-time dashboard
   - Export attendance reports
   - Mobile app deep linking

4. **Deployment:**
   - Set up cloud database
   - Deploy backend to server
   - Publish app to App Store/Play Store

## Useful Commands

```bash
# Backend
python api_server.py                           # Start API server
curl http://localhost:5000/                    # Health check
curl http://localhost:5000/persons             # Get employees

# Frontend
npm start                                      # Start Expo
npm run android                               # Run Android
npm run ios                                   # Run iOS
npm run web                                   # Run Web
npm install <package>                         # Install package
npm audit                                     # Check security

# Database
sqlite3 attendance.db ".tables"                # List tables
sqlite3 attendance.db "SELECT COUNT(*) FROM attendance;"  # Count records

# Virtual Environment
python -m venv venv                           # Create env
source venv/bin/activate                      # Activate (Linux/Mac)
venv\Scripts\activate                         # Activate (Windows)
deactivate                                    # Deactivate
```

## Getting Help

If you encounter issues:

1. **Check Logs:**
   - Backend: Look at terminal output
   - Frontend: Check Expo console
   - Database: Use SQLite browser

2. **Debug:**
   - Use React Native debugger
   - Add console.log statements
   - Use Flask debug mode

3. **Documentation:**
   - See BACKEND_INTEGRATION.md
   - See frontend/README.md
   - Check individual service files

4. **Common Issues:**
   - Network connectivity
   - Port conflicts
   - Permission issues
   - Missing dependencies

## License

This project is part of the Hackathon Face Detection initiative.

## Support

Contact the development team for additional support.
