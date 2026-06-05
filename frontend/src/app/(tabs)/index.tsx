import React, { useState, useCallback, useRef } from 'react';
import {
  StyleSheet, Text, View, FlatList, TouchableOpacity,
  Modal, TextInput, Alert, ActivityIndicator,
} from 'react-native';
import { CameraView, useCameraPermissions } from 'expo-camera';
import { useFocusEffect } from 'expo-router';
import { DatabaseService, AttendanceRecord } from '../../services/DatabaseService';
import { syncService } from '../../services/SyncService';
import { faceRecognitionService } from '../../services/FaceRecognitionService';

export default function HomeScreen() {
  const [records, setRecords] = useState<AttendanceRecord[]>([]);
  const [stats, setStats] = useState({ total: 0, synced: 0, unsynced: 0 });

  // Registration state
  const [registerVisible, setRegisterVisible] = useState(false);
  const [personId, setPersonId] = useState('');
  const [registering, setRegistering] = useState(false);
  const [cameraReady, setCameraReady] = useState(false);
  const cameraRef = useRef<CameraView>(null);
  const [permission, requestPermission] = useCameraPermissions();

  const loadData = useCallback(async () => {
    try {
      const [fetched, s] = await Promise.all([
        DatabaseService.getAttendance(50),
        DatabaseService.getStats(),
      ]);
      setRecords(fetched);
      setStats(s);
    } catch (e) {
      console.error('Failed to load attendance data:', e);
    }
  }, []);

  useFocusEffect(
    useCallback(() => {
      loadData();
    }, [loadData])
  );

  const handleSyncNow = async () => {
    try {
      await syncService.syncAttendance();
      await loadData();
    } catch (e) {
      console.error('Sync failed:', e);
    }
  };

  const handleOpenRegister = async () => {
    if (!permission?.granted) {
      const { granted } = await requestPermission();
      if (!granted) {
        Alert.alert('Permission required', 'Camera access is needed to register a face.');
        return;
      }
    }
    setPersonId('');
    setCameraReady(false);
    setRegisterVisible(true);
  };

  const handleRegister = async () => {
    if (!personId.trim()) {
      Alert.alert('Name required', 'Please enter a name or ID before registering.');
      return;
    }
    if (!cameraRef.current) {
      Alert.alert('Camera not ready', 'Please wait for the camera to initialize.');
      return;
    }

    setRegistering(true);
    try {
      const base64 = await faceRecognitionService.captureFrameBase64(cameraRef.current);
      if (!base64) {
        Alert.alert('Capture failed', 'Could not capture photo. Please try again.');
        return;
      }

      const result = await faceRecognitionService.registerFace(base64, personId.trim());
      if (result.success) {
        Alert.alert('Registered!', `${personId.trim()} has been registered successfully.`);
        setRegisterVisible(false);
      } else {
        Alert.alert('Registration failed', result.message || 'No face detected. Please try again.');
      }
    } catch (e: any) {
      Alert.alert('Error', e?.message ?? 'Something went wrong.');
    } finally {
      setRegistering(false);
    }
  };

  const renderItem = ({ item }: { item: AttendanceRecord }) => (
    <View style={styles.card}>
      <View style={styles.avatar}>
        <Text style={styles.avatarText}>
          {(item.person_name || item.person_id)[0]?.toUpperCase() ?? '?'}
        </Text>
      </View>
      <View style={styles.info}>
        <Text style={styles.nameText}>{item.person_name || item.person_id}</Text>
        <Text style={styles.timeText}>
          {new Date(item.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
        </Text>
      </View>
      <View style={styles.rightCol}>
        <View style={[styles.statusPill, item.synced ? styles.syncedPill : styles.unsyncedPill]}>
          <Text style={[styles.statusText, item.synced ? styles.syncedText : styles.unsyncedText]}>
            {item.synced ? 'Synced' : 'Pending'}
          </Text>
        </View>
        <Text style={styles.confidenceText}>{(item.confidence * 100).toFixed(0)}%</Text>
      </View>
    </View>
  );

  return (
    <View style={styles.container}>
      {/* ── Stats bar ── */}
      <View style={styles.statsBar}>
        <View style={styles.stat}>
          <Text style={styles.statValue}>{stats.total}</Text>
          <Text style={styles.statLabel}>Total</Text>
        </View>
        <View style={styles.stat}>
          <Text style={styles.statValue}>{stats.synced}</Text>
          <Text style={styles.statLabel}>Synced</Text>
        </View>
        <View style={styles.stat}>
          <Text style={[styles.statValue, stats.unsynced > 0 && { color: '#FF6B6B' }]}>
            {stats.unsynced}
          </Text>
          <Text style={styles.statLabel}>Pending</Text>
        </View>
        <TouchableOpacity style={styles.syncButton} onPress={handleSyncNow}>
          <Text style={styles.syncButtonText}>Sync</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.registerButton} onPress={handleOpenRegister}>
          <Text style={styles.registerButtonText}>+ Register</Text>
        </TouchableOpacity>
      </View>

      {/* ── Attendance list ── */}
      <FlatList
        data={records}
        keyExtractor={(item) => item.id.toString()}
        contentContainerStyle={styles.listContainer}
        ListEmptyComponent={
          <View style={styles.emptyContainer}>
            <Text style={styles.emptyTitle}>No Attendance Yet</Text>
            <Text style={styles.emptySubtitle}>Go to Mark Attendance tab to start</Text>
          </View>
        }
        renderItem={renderItem}
      />

      {/* ── Register modal ── */}
      <Modal visible={registerVisible} animationType="slide" onRequestClose={() => setRegisterVisible(false)}>
        <View style={styles.modalContainer}>
          <Text style={styles.modalTitle}>Register New Face</Text>

          <CameraView
            ref={cameraRef}
            style={styles.camera}
            facing="front"
            onCameraReady={() => setCameraReady(true)}
          />

          <TextInput
            style={styles.input}
            placeholder="Enter name or employee ID"
            placeholderTextColor="#999"
            value={personId}
            onChangeText={setPersonId}
            autoCapitalize="words"
          />

          <TouchableOpacity
            style={[styles.captureButton, (!cameraReady || registering) && styles.captureButtonDisabled]}
            onPress={handleRegister}
            disabled={!cameraReady || registering}
          >
            {registering
              ? <ActivityIndicator color="#fff" />
              : <Text style={styles.captureButtonText}>Capture & Register</Text>
            }
          </TouchableOpacity>

          <TouchableOpacity style={styles.cancelButton} onPress={() => setRegisterVisible(false)}>
            <Text style={styles.cancelButtonText}>Cancel</Text>
          </TouchableOpacity>
        </View>
      </Modal>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  statsBar: {
    flexDirection: 'row',
    backgroundColor: '#fff',
    padding: 12,
    margin: 12,
    borderRadius: 12,
    alignItems: 'center',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 3,
    elevation: 2,
  },
  stat: { flex: 1, alignItems: 'center' },
  statValue: { fontSize: 22, fontWeight: 'bold', color: '#333' },
  statLabel: { fontSize: 11, color: '#999', marginTop: 2 },
  syncButton: {
    backgroundColor: '#007AFF',
    paddingVertical: 8,
    paddingHorizontal: 16,
    borderRadius: 8,
    marginRight: 6,
  },
  syncButtonText: { color: '#fff', fontSize: 13, fontWeight: '600' },
  registerButton: {
    backgroundColor: '#34C759',
    paddingVertical: 8,
    paddingHorizontal: 12,
    borderRadius: 8,
  },
  registerButtonText: { color: '#fff', fontSize: 13, fontWeight: '600' },
  listContainer: { padding: 12, paddingTop: 0, flexGrow: 1 },
  card: {
    flexDirection: 'row',
    backgroundColor: '#fff',
    padding: 12,
    borderRadius: 12,
    alignItems: 'center',
    marginBottom: 10,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 3,
    elevation: 2,
  },
  avatar: {
    width: 44, height: 44, borderRadius: 22,
    backgroundColor: '#007AFF', justifyContent: 'center', alignItems: 'center',
  },
  avatarText: { color: '#fff', fontSize: 18, fontWeight: 'bold' },
  info: { flex: 1, marginLeft: 12 },
  nameText: { fontSize: 15, fontWeight: 'bold', color: '#333' },
  timeText: { fontSize: 12, color: '#666', marginTop: 2 },
  rightCol: { alignItems: 'flex-end' },
  statusPill: { paddingHorizontal: 8, paddingVertical: 3, borderRadius: 12, marginBottom: 4 },
  syncedPill: { backgroundColor: '#E8F5E9' },
  unsyncedPill: { backgroundColor: '#FFF3E0' },
  statusText: { fontSize: 11, fontWeight: '600' },
  syncedText: { color: '#2E7D32' },
  unsyncedText: { color: '#E65100' },
  confidenceText: { fontSize: 12, color: '#999' },
  emptyContainer: { flex: 1, justifyContent: 'center', alignItems: 'center', marginTop: 80 },
  emptyTitle: { fontSize: 18, fontWeight: 'bold', color: '#999' },
  emptySubtitle: { fontSize: 14, color: '#bbb', marginTop: 6 },

  // Modal
  modalContainer: {
    flex: 1, backgroundColor: '#000', alignItems: 'center',
    justifyContent: 'center', padding: 24,
  },
  modalTitle: {
    color: '#fff', fontSize: 22, fontWeight: 'bold', marginBottom: 20,
  },
  camera: {
    width: 280, height: 320, borderRadius: 16, overflow: 'hidden', marginBottom: 24,
  },
  input: {
    width: '100%', backgroundColor: '#1c1c1e', color: '#fff',
    borderRadius: 10, padding: 14, fontSize: 16, marginBottom: 16,
    borderWidth: 1, borderColor: '#333',
  },
  captureButton: {
    width: '100%', backgroundColor: '#34C759',
    paddingVertical: 16, borderRadius: 12, alignItems: 'center', marginBottom: 12,
  },
  captureButtonDisabled: { backgroundColor: '#555' },
  captureButtonText: { color: '#fff', fontSize: 16, fontWeight: '700' },
  cancelButton: {
    width: '100%', paddingVertical: 14, alignItems: 'center',
  },
  cancelButtonText: { color: '#FF3B30', fontSize: 16 },
});