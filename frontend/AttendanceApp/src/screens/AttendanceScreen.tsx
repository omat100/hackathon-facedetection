import React, { useState, useEffect } from 'react';
import {
  View,
  StyleSheet,
  TouchableOpacity,
  Text,
  Alert,
  SafeAreaView,
  StatusBar,
  ScrollView,
} from 'react-native';
import { CameraView } from 'expo-camera';
import { databaseService } from '../services/DatabaseService';
import { faceRecognitionService } from '../services/FaceRecognitionService';
import { networkService } from '../services/NetworkService';
import { syncService } from '../services/SyncService';
import { AttendanceRecord, AppState } from '../types';
import { LivenessPrompt } from '../components/LivenessPrompt';
import { SuccessScreen } from '../components/SuccessScreen';
import { FailureScreen } from '../components/FailureScreen';
import { SearchBar } from '../components/SearchBar';
import { AttendanceList } from '../components/AttendanceList';

export const AttendanceScreen: React.FC = () => {
  const [appState, setAppState] = useState<AppState>(AppState.IDLE);
  const [livenessProgress, setLivenessProgress] = useState(0);
  const [attendanceRecords, setAttendanceRecords] = useState<AttendanceRecord[]>(
    []
  );
  const [isOnline, setIsOnline] = useState(false);
  const [searchText, setSearchText] = useState('');
  const [persons, setPersons] = useState<Array<{ id: string; name: string }>>([
    ]);
  const [lastResult, setLastResult] = useState<{
    personName: string;
    confidence: number;
    timestamp: string;
  } | null>(null);
  const [loading, setLoading] = useState(false);
  const [cameraActive, setCameraActive] = useState(false);

  // Initialize services
  useEffect(() => {
    const initialize = async () => {
      try {
        await databaseService.initialize();
        await networkService.initialize();

        // Load attendance records
        await loadAttendanceRecords();

        // Load persons
        loadPersons();

        // Subscribe to network changes
        const unsubscribe = networkService.subscribe((online) => {
          setIsOnline(online);
          if (online) {
            handleSync();
          }
        });

        return unsubscribe;
      } catch (error) {
        console.error('Initialization error:', error);
      }
    };

    initialize();
  }, []);

  const loadAttendanceRecords = async () => {
    try {
      const records = await databaseService.getAttendanceRecords();
      setAttendanceRecords(records);
    } catch (error) {
      console.error('Error loading records:', error);
    }
  };

  const loadPersons = async () => {
    try {
      // Try to fetch from backend first
      if (isOnline) {
        const personsData = await syncService.fetchPersonDatabase();
        if (personsData.length > 0) {
          setPersons(personsData);
          for (const person of personsData) {
            await databaseService.savePerson(person.id, person.name);
          }
          return;
        }
      }

      // Fall back to mock database
      const mockPersons = faceRecognitionService.getMockDatabase();
      setPersons(mockPersons);
    } catch (error) {
      console.error('Error loading persons:', error);
    }
  };

  const handleMarkAttendance = async () => {
    if (!cameraActive) {
      setCameraActive(true);
      setAppState(AppState.LIVENESS_CHECK);
      simulateLivenessCheck();
    }
  };

  const simulateLivenessCheck = () => {
    let progress = 0;
    const interval = setInterval(() => {
      progress += Math.random() * 15;
      if (progress >= 100) {
        progress = 100;
        clearInterval(interval);
        handleFaceRecognition();
      }
      setLivenessProgress(Math.min(progress, 100));
    }, 500);
  };

  const handleFaceRecognition = async () => {
    setAppState(AppState.FACE_DETECTION);

    try {
      // Simulate face recognition
      const result = await faceRecognitionService.recognizeFace(null);

      if (result && result.livenessVerified) {
        // Log attendance
        await databaseService.logAttendance(
          result.personId,
          result.confidence
        );

        // Update UI
        setLastResult({
          personName: result.personName,
          confidence: result.confidence,
          timestamp: new Date().toISOString(),
        });

        setAppState(AppState.SUCCESS);

        // Reload records
        await loadAttendanceRecords();

        // Try to sync if online
        if (isOnline) {
          handleSync();
        }
      } else {
        setAppState(AppState.FAILED);
      }
    } catch (error) {
      console.error('Face recognition error:', error);
      setAppState(AppState.FAILED);
    }
  };

  const handleSync = async () => {
    try {
      const unsyncedRecords = await databaseService.getUnsyncedRecords();

      if (unsyncedRecords.length === 0) {
        return;
      }

      const result = await syncService.syncAttendance(unsyncedRecords);

      if (result.success && result.synced > 0) {
        const ids = unsyncedRecords
          .slice(0, result.synced)
          .map((r) => r.id)
          .filter((id) => id !== undefined) as number[];

        await databaseService.markSynced(ids);
        await loadAttendanceRecords();
        Alert.alert('Sync Success', `${result.synced} records synced`);
      }
    } catch (error) {
      console.error('Sync error:', error);
    }
  };

  const handleDismiss = () => {
    setAppState(AppState.IDLE);
    setCameraActive(false);
    setLivenessProgress(0);
    setLastResult(null);
    setSearchText('');
  };

  const handleRetry = () => {
    setAppState(AppState.LIVENESS_CHECK);
    setLivenessProgress(0);
    simulateLivenessCheck();
  };

  return (
    <SafeAreaView style={styles.container}>
      <StatusBar barStyle="dark-content" backgroundColor="#fff" />

      {cameraActive && (
        <View style={styles.cameraContainer}>
          <View style={styles.cameraPlaceholder}>
            <Text style={styles.cameraText}>📷 Camera View</Text>
            <Text style={styles.cameraSubtext}>
              (Simulated camera for demo)
            </Text>
          </View>

          <TouchableOpacity
            style={styles.closeButton}
            onPress={handleDismiss}
            activeOpacity={0.7}
          >
            <Text style={styles.closeButtonText}>✕</Text>
          </TouchableOpacity>

          <LivenessPrompt state={appState} progress={livenessProgress} />

          {lastResult && appState === AppState.SUCCESS && (
            <SuccessScreen
              personName={lastResult.personName}
              confidence={lastResult.confidence}
              timestamp={lastResult.timestamp}
              onDismiss={handleDismiss}
            />
          )}

          {appState === AppState.FAILED && (
            <FailureScreen
              message="Could not detect face or verify liveness. Please try again."
              onRetry={handleRetry}
              onCancel={handleDismiss}
            />
          )}
        </View>
      )}

      {!cameraActive && (
        <ScrollView style={styles.content} showsVerticalScrollIndicator={false}>
          {/* Status Bar */}
          <View style={styles.statusBar}>
            <View
              style={[
                styles.statusBadge,
                {
                  backgroundColor: isOnline ? '#4CAF50' : '#FF9800',
                },
              ]}
            >
              <Text style={styles.statusText}>
                {isOnline ? '🌐 Online' : '📵 Offline'}
              </Text>
            </View>
            <TouchableOpacity onPress={handleSync} disabled={!isOnline}>
              <Text
                style={[
                  styles.syncButton,
                  { opacity: isOnline ? 1 : 0.5 },
                ]}
              >
                🔄 Sync
              </Text>
            </TouchableOpacity>
          </View>

          {/* Search Bar */}
          <SearchBar
            value={searchText}
            onChangeText={setSearchText}
            onPress={handleMarkAttendance}
            searchResults={persons}
          />

          {/* Attendance List */}
          <View style={styles.listContainer}>
            <Text style={styles.sectionTitle}>Attendance Records</Text>
            <AttendanceList
              records={attendanceRecords}
              loading={loading}
              onRefresh={loadAttendanceRecords}
            />
          </View>
        </ScrollView>
      )}
    </SafeAreaView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#fff',
  },
  cameraContainer: {
    flex: 1,
    backgroundColor: '#000',
    position: 'relative',
  },
  cameraPlaceholder: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  cameraText: {
    fontSize: 24,
    color: '#fff',
    marginBottom: 8,
  },
  cameraSubtext: {
    fontSize: 12,
    color: '#999',
  },
  closeButton: {
    position: 'absolute',
    top: 16,
    right: 16,
    width: 44,
    height: 44,
    borderRadius: 22,
    backgroundColor: 'rgba(255,255,255,0.3)',
    justifyContent: 'center',
    alignItems: 'center',
    zIndex: 50,
  },
  closeButtonText: {
    fontSize: 24,
    color: '#fff',
  },
  content: {
    flex: 1,
  },
  statusBar: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: 16,
    paddingVertical: 12,
    backgroundColor: '#f9f9f9',
    borderBottomWidth: 1,
    borderBottomColor: '#eee',
  },
  statusBadge: {
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 12,
  },
  statusText: {
    color: '#fff',
    fontSize: 12,
    fontWeight: '600',
  },
  syncButton: {
    color: '#2196F3',
    fontSize: 12,
    fontWeight: '600',
    paddingHorizontal: 12,
    paddingVertical: 6,
  },
  listContainer: {
    flex: 1,
    minHeight: 400,
    paddingHorizontal: 0,
  },
  sectionTitle: {
    fontSize: 16,
    fontWeight: '600',
    color: '#333',
    paddingHorizontal: 16,
    paddingTop: 16,
    paddingBottom: 8,
  },
});
