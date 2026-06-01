# Face Attendance Detection System

A comprehensive face recognition-based attendance system with offline support, real-time synchronization, and mobile app integration.

## 🎯 Overview

This is a complete solution for marking employee attendance using facial recognition and liveness detection. It combines:

- **Python Backend** - Face recognition, liveness detection, and attendance processing
- **React Native Mobile App** - Cross-platform app for Android and iOS
- **REST API** - Flask-based bridge for frontend-backend communication
- **Offline-First** - Works without internet, syncs when available

## ✨ Key Features

### Backend
- ✅ **Face Recognition** - InsightFace-based deep learning model
- ✅ **Liveness Detection** - MediaPipe face landmarking for anti-spoofing
- ✅ **C++ Optimizations** - Fast face selection from multiple detections
- ✅ **SQLite Storage** - Local database with sync capability
- ✅ **AWS Integration** - Cloud sync and backup

### Mobile App (React Native)
- ✅ **Cross-Platform** - Native iOS and Android support
- ✅ **Face Detection** - Real-time camera integration
- ✅ **Offline Mode** - Complete functionality without internet
- ✅ **Auto Sync** - Automatic data synchronization when online
- ✅ **Attendance History** - View all records with timestamps
- ✅ **Search** - Quick employee lookup
- ✅ **Liveness Prompts** - User guidance during detection
- ✅ **Success/Failure Screens** - Clear visual feedback
- ✅ **Statistics Dashboard** - Database and sync stats

### API Server
- ✅ **REST Endpoints** - Complete API for frontend-backend communication
- ✅ **CORS Support** - Cross-origin requests enabled
- ✅ **File Upload** - Secure image upload handling
- ✅ **Batch Sync** - Efficient record synchronization
- ✅ **Rate Limiting** - Protection against abuse

## 📁 Project Structure

```
hackathon-facedetection/
├── Backend Services
├── main_pipeline.py              # Core attendance pipeline
├── face_recognition_engine.py    # Face recognition logic
├── liveness_detection.py         # Liveness detection
├── storage.py                    # SQLite database
├── api_server.py                 # Flask API server
│
├── Configuration & Documentation
├── requirements.txt              # Python dependencies
├── app.json                      # App configuration
├── README.md                     # This file
├── QUICKSTART.md                 # Quick start guide
├── BACKEND_INTEGRATION.md        # Integration details
├── API_SPEC.md                   # API specification
├── DEPLOYMENT.md                 # Deployment guide
│
├── Support Services
├── preprocessing/
│   └── preprocessing.py          # Image preprocessing
├── training/
│   └── train.py                  # Model training
├── cpp/
│   └── liveness_engine/          # C++ optimizations
├── aws_sync.py                   # AWS integration
│
└── Frontend (React Native)
    └── frontend/
        ├── README.md             # Frontend documentation
        └── AttendanceApp/
            ├── src/
            │   ├── screens/      # App screens
            │   ├── components/   # Reusable UI components
            │   ├── services/     # Business logic services
            │   └── types/        # TypeScript definitions
            ├── App.tsx           # Main app component
            ├── app.json          # Expo configuration
            └── package.json      # NPM dependencies
```

## 🚀 Quick Start

### Prerequisites
- Python 3.8+
- Node.js 16+
- npm

### 1. Backend Setup

```bash
# Create virtual environment
python -m venv venv
source venv/bin/activate  # or venv\Scripts\activate on Windows

# Install dependencies
pip install -r requirements.txt

# Start API server
python api_server.py
```

### 2. Frontend Setup

```bash
# Navigate to frontend
cd frontend/AttendanceApp

# Install dependencies
npm install

# Start development server
npm start
```

### 3. Run on Device

**With Expo Go (simplest):**
- Install Expo Go app on your phone
- Scan the QR code from npm start
- App opens automatically

**With Android Emulator:**
```bash
npm run android
```

**With iOS Simulator (macOS):**
```bash
npm run ios
```

**On Web Browser:**
```bash
npm run web
```

## 📚 Documentation

| Document | Purpose |
|----------|---------|
| [QUICKSTART.md](./QUICKSTART.md) | Get up and running in minutes |
| [BACKEND_INTEGRATION.md](./BACKEND_INTEGRATION.md) | Detailed integration guide |
| [API_SPEC.md](./API_SPEC.md) | Complete API endpoint documentation |
| [DEPLOYMENT.md](./DEPLOYMENT.md) | Production deployment guide |
| [frontend/README.md](./frontend/README.md) | Mobile app documentation |

## 🔌 API Integration

The app communicates with the backend through REST API:

**Main Endpoints:**
- `POST /recognize` - Face recognition
- `POST /liveness-check` - Liveness verification
- `POST /sync-attendance` - Data synchronization
- `GET /persons` - Get employee list
- `GET /attendance-stats` - Get statistics

See [API_SPEC.md](./API_SPEC.md) for complete documentation.

## 🏗️ Architecture

```
Frontend (React Native)
    ↓ HTTP REST API
API Server (Flask)
    ↓
Backend Services
    ├── Face Recognition (InsightFace)
    ├── Liveness Detection (MediaPipe)
    └── Storage (SQLite)
```

## 📱 App Workflow

1. **User opens app** → Load attendance records from local SQLite
2. **User taps "Mark Attendance"** → Camera opens
3. **Liveness detection** → Follow on-screen prompts (look, turn head, smile)
4. **Face recognition** → Match against database
5. **Mark attendance** → Save to SQLite locally
6. **If online** → Automatically sync to backend
7. **Show result** → Success/failure screen with confidence score

## 🔒 Security Features

- **Liveness Detection** - Prevents photo/mask spoofing
- **Offline-First** - No unnecessary data transmission
- **SQLite Encryption** - Local data protection (optional)
- **HTTPS** - Secure API communication
- **Rate Limiting** - API abuse prevention
- **Input Validation** - Prevent injection attacks

## 💾 Database

### Local (SQLite)
- **attendance** - Local attendance records
- **persons** - Employee database
- **sync_log** - Synchronization history

### Cloud (Optional)
- AWS RDS for backup
- CloudWatch for monitoring
- S3 for file storage

## ⚙️ Configuration

### Backend
Edit `api_server.py`:
```python
private backendUrl: string = 'http://your-server:5000';
```

### Frontend
Edit `src/services/FaceRecognitionService.ts`:
```typescript
private backendUrl: string = 'http://your-server:5000';
```

## 🔄 Offline & Sync

### Offline Mode
- App records attendance locally
- Uses SQLite for persistence
- Shows "📵 Offline" status
- Queues records for later sync

### Online Mode
- Shows "🌐 Online" status
- Automatically syncs pending records
- Updates employee database
- Batch processes for efficiency

### Sync Mechanism
1. Detects network change
2. Fetches pending records
3. Batches them (50 at a time)
4. Sends to backend
5. Marks as synced on confirmation

## 📊 Performance

- **Face Recognition** - ~200-500ms per frame
- **Liveness Detection** - ~100-300ms per frame
- **API Response** - <500ms average
- **Sync** - <2s for batch of 50 records
- **App Launch** - <3s on modern devices

## 🧪 Testing

### Backend
```bash
# Manual testing
curl http://localhost:5000/

# Test with image
curl -X POST -F "frame=@test.jpg" http://localhost:5000/recognize
```

### Frontend
```bash
# Run tests
npm test

# Build for production
npm run build
```

## 🚢 Deployment

### Development
```bash
python api_server.py  # Backend
npm start             # Frontend
```

### Production
See [DEPLOYMENT.md](./DEPLOYMENT.md) for:
- Docker containerization
- Kubernetes deployment
- AWS/GCP setup
- CI/CD pipeline
- Scaling & monitoring

## 📈 Scaling

- **Horizontal Scaling** - Multiple API instances
- **Caching** - Redis for frequently accessed data
- **Load Balancing** - Nginx/HAProxy
- **Database** - Connection pooling
- **CDN** - For static assets (optional)

## 🔧 Troubleshooting

### Connection Issues
```bash
# Check backend
curl http://localhost:5000/

# Check network
ping your-server

# Update frontend URL
# See configuration section above
```

### Recognition Issues
- Ensure good lighting
- Face should be front-facing
- Clear camera lens
- Check backend is running

### Database Issues
- Clear app cache
- Uninstall and reinstall app
- Use "Clear All Data" in Settings

See documentation files for more troubleshooting.

## 🤝 Contributing

1. Fork repository
2. Create feature branch
3. Commit changes
4. Push to branch
5. Create Pull Request

## 📝 License

This project is part of the Hackathon Face Detection initiative.

## 📞 Support

For issues, questions, or feedback:
1. Check [QUICKSTART.md](./QUICKSTART.md)
2. Review [BACKEND_INTEGRATION.md](./BACKEND_INTEGRATION.md)
3. Check [API_SPEC.md](./API_SPEC.md)
4. Contact development team

## 🎓 Technologies Used

### Backend
- Python 3.8+
- Flask - Web framework
- InsightFace - Face recognition
- MediaPipe - Liveness detection
- OpenCV - Image processing
- SQLite - Database

### Frontend
- React Native
- Expo
- TypeScript
- React Navigation
- React Native Vision Camera

### DevOps
- Docker
- Docker Compose
- Kubernetes (optional)
- Nginx
- Gunicorn

### Cloud
- AWS (EC2, RDS, S3)
- Azure (App Service, SQL DB)
- GCP (Cloud Run, Cloud SQL)

## 📊 Statistics

- **Backend Code** - ~1000 lines of Python
- **Frontend Code** - ~3000 lines of TypeScript/React
- **API Endpoints** - 8+ REST endpoints
- **Database Tables** - 3 main tables
- **Supported Devices** - iOS 12+, Android 6+

## 🎯 Future Enhancements

- [ ] Multi-face detection in single frame
- [ ] Real-time dashboard
- [ ] Attendance analytics
- [ ] Biometric authentication
- [ ] SMS/Email notifications
- [ ] Department-wise reports
- [ ] Holiday management
- [ ] Shift tracking

## 📅 Release Notes

### v1.0.0 (Current)
- Initial release
- Complete offline support
- Face recognition
- Liveness detection
- Mobile app
- REST API

### Upcoming
- v1.1.0 - Analytics dashboard
- v1.2.0 - Advanced reporting
- v2.0.0 - Mobile app improvements

## 🌟 Acknowledgments

Built with ❤️ for the Hackathon Face Detection initiative.

Special thanks to:
- InsightFace team for face recognition models
- MediaPipe team for liveness detection
- Expo team for React Native framework
- Open source community

---

**Ready to get started?** See [QUICKSTART.md](./QUICKSTART.md) for step-by-step setup instructions.
