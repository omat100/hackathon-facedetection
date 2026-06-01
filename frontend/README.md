# Face Attendance Mobile App

A React Native mobile application for marking employee attendance using facial recognition and liveness detection. The app works offline with automatic syncing when internet is available.

## Features

- ✅ **Face Recognition** - Detects and recognizes employee faces
- ✅ **Liveness Detection** - Anti-spoofing checks to prevent fraudulent attendance
- ✅ **Offline Support** - Works completely offline with SQLite database
- ✅ **Auto Sync** - Automatically syncs data with backend when online
- ✅ **Search Functionality** - Search employees by name
- ✅ **Real-time Status** - Shows online/offline status
- ✅ **Cross-platform** - Works on both Android and iOS
- ✅ **Attendance History** - View all attendance records with timestamps and confidence scores
- ✅ **Beautiful UI** - Clean, intuitive interface with visual feedback

## Project Structure

```
src/
├── components/        # Reusable UI components
│   ├── AttendanceList.tsx      # Displays attendance history
│   ├── FailureScreen.tsx        # Failure notification screen
│   ├── LivenessPrompt.tsx       # Liveness check prompts
│   ├── SearchBar.tsx            # Search and mark attendance button
│   └── SuccessScreen.tsx        # Success notification screen
├── screens/           # Main app screens
│   ├── AttendanceScreen.tsx     # Main attendance marking screen
│   └── SettingsScreen.tsx       # Settings and statistics
├── services/          # Business logic
│   ├── DatabaseService.ts       # SQLite operations
│   ├── FaceRecognitionService.ts # Face detection and recognition
│   ├── NetworkService.ts        # Network status monitoring
│   └── SyncService.ts           # Backend synchronization
├── types/             # TypeScript type definitions
│   └── index.ts       # All types and enums
└── utils/             # Utility functions
```

## Tech Stack

- **Framework**: React Native with Expo
- **Language**: TypeScript
- **Database**: SQLite (expo-sqlite)
- **Navigation**: React Navigation (Bottom Tabs)
- **Camera**: expo-camera, react-native-vision-camera
- **ML Models**: TensorFlow Lite (via react-native-fast-tflite)
- **Network**: @react-native-community/netinfo

## Installation

### Prerequisites

- Node.js 16+ and npm
- Expo CLI: `npm install -g expo-cli`
- XCode (for iOS) or Android Studio (for Android)

### Setup

1. **Install dependencies:**

```bash
cd frontend/AttendanceApp
npm install
```

2. **Install additional packages** (if not already installed):

```bash
npm install expo-camera react-native-vision-camera expo-sqlite react-native-fast-tflite
npm install @react-navigation/native @react-navigation/bottom-tabs react-native-screens react-native-safe-area-context
npm install @react-native-community/netinfo
```

3. **Start the development server:**

```bash
npm start
```

### Running on Devices

**Android:**
```bash
npm run android
```

**iOS:**
```bash
npm run ios
```

**Web:**
```bash
npm run web
```

## API Integration

### Backend Connection

The app connects to the Python backend through HTTP APIs. Configure the backend URL in `src/services/FaceRecognitionService.ts`:

```typescript
private backendUrl: string = 'http://localhost:5000'; // Change to your backend URL
```

### Supported Endpoints

#### Face Recognition
```
POST /recognize
Body: FormData with frame image
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

#### Liveness Check
```
POST /liveness-check
Body: FormData with frame image
Response: {
  "verified": true,
  "confidence": 0.92
}
```

#### Get Persons Database
```
GET /persons
Response: {
  "persons": [
    {"id": "emp001", "name": "John Doe"},
    {"id": "emp002", "name": "Jane Smith"}
  ]
}
```

#### Sync Attendance
```
POST /sync-attendance
Body: {
  "records": [
    {
      "id": 1,
      "person_id": "emp001",
      "timestamp": "2024-06-01T10:30:00.000Z",
      "confidence": 0.95,
      "synced": false
    }
  ],
  "timestamp": "2024-06-01T10:30:00.000Z",
  "client_info": {
    "platform": "react-native",
    "version": "1.0.0"
  }
}
Response: { "status": "success" }
```

## Database Schema

### SQLite Tables

**attendance**
```sql
CREATE TABLE attendance (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  person_id TEXT NOT NULL,
  timestamp TEXT NOT NULL,
  confidence REAL NOT NULL,
  synced INTEGER DEFAULT 0
)
```

**persons**
```sql
CREATE TABLE persons (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL,
  embedding TEXT,
  last_sync TEXT
)
```

**sync_log**
```sql
CREATE TABLE sync_log (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  batch_id TEXT,
  timestamp TEXT,
  record_count INTEGER,
  success INTEGER
)
```

## Usage

### 1. Mark Attendance

1. Open the app and go to the "Attendance" tab
2. View the list of today's attendance records
3. Tap "Mark Attendance" button
4. Follow the camera prompts:
   - Look straight at camera
   - Turn head left
   - Turn head right
   - Smile for camera
5. System verifies liveness and matches face
6. Attendance is recorded and synced (if online)

### 2. Search Employee

1. Use the search bar to find a person by name
2. As you type, matching results appear below
3. Tap on a person to view details
4. Tap "Mark Attendance" to start the process

### 3. View Attendance Records

- All attendance records are shown in chronological order
- Pull down to refresh from local database
- Green badge shows synced records
- Orange badge shows pending sync
- Confidence percentage indicates face match accuracy

### 4. Sync Data

- Tap the "🔄 Sync" button to manually sync pending records
- Automatic sync happens when device goes online
- Synced records are marked with ✓ badge

### 5. View Statistics

- Go to "Settings" tab
- View database statistics:
  - Total records
  - Synced count
  - Pending sync count
- View app information and features

## Offline Mode

When offline:
- App continues to record attendance locally
- Uses mock face database for demo purposes
- Shows "📵 Offline" status
- Records are stored in SQLite for later sync
- No "Sync" button available

When online:
- Shows "🌐 Online" status
- Automatically syncs pending records
- Updates person database from backend
- Uses real backend for face recognition

## Development

### Adding New Features

1. **Create new screen:**
   - Add `.tsx` file in `src/screens/`
   - Add screen to navigation in `App.tsx`

2. **Create new service:**
   - Add `.ts` file in `src/services/`
   - Export service instance for use in components

3. **Create new component:**
   - Add `.tsx` file in `src/components/`
   - Import and use in screens

4. **Add types:**
   - Update `src/types/index.ts` with new type definitions

### Building for Production

**Android:**
```bash
eas build --platform android
```

**iOS:**
```bash
eas build --platform ios
```

## Troubleshooting

### Camera Permission Issues

**Android:**
- Ensure camera permission is granted in app settings
- Check `android.permissions` in `app.json`

**iOS:**
- Check NSCameraUsageDescription in `app.json` ios.infoPlist

### Database Errors

- Clear app cache: Settings > Apps > Clear Cache
- Delete app data: Settings > Apps > Storage > Clear Storage
- Use "Clear All Data" button in Settings screen

### Sync Issues

- Check backend URL configuration
- Ensure backend server is running
- Check network connectivity
- Review sync logs in database

### Face Recognition Not Working

- Ensure good lighting
- Look directly at camera
- Complete all liveness prompts
- Backend must be running for real recognition

## Performance Optimization

- Uses SQLite for fast local queries
- Implements batch sync for efficient networking
- Lazy loads attendance records
- Optimizes re-renders with React hooks
- Implements proper cleanup in useEffect hooks

## Security

- Implements liveness detection to prevent spoofing
- Offline mode doesn't expose sensitive data
- Uses local SQLite without encryption (can be added)
- Implements network status checks before syncing
- Confidence threshold for valid attendance

## Future Enhancements

- [ ] TensorFlow Lite model integration for on-device ML
- [ ] Face embeddings storage for faster matching
- [ ] Multi-face detection in single frame
- [ ] Attendance reports and analytics
- [ ] Push notifications for attendance confirmation
- [ ] Biometric authentication (fingerprint/face unlock)
- [ ] Multi-language support
- [ ] Dark mode support
- [ ] Attendance retry logic with backoff
- [ ] CSV export functionality

## License

This project is part of the Hackathon Face Detection initiative.

## Support

For issues or questions, contact the development team.
