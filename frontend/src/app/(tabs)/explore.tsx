import React, { useRef, useState, useCallback } from 'react';
import { StyleSheet, Text, View, TouchableOpacity, ActivityIndicator } from 'react-native';
import { Camera, useCameraDevice, useCameraPermission } from 'react-native-vision-camera';
import { faceRecognitionService } from '../../services/FaceRecognitionService';

type Stage = 'idle' | 'liveness' | 'recognizing' | 'success' | 'failed';

export default function MarkAttendanceScreen() {
  const device = useCameraDevice('front');
  const { hasPermission, requestPermission } = useCameraPermission();
  const cameraRef = useRef<Camera>(null);
  const [stage, setStage] = useState<Stage>('idle');
  const [message, setMessage] = useState('');
  const [progress, setProgress] = useState(0);
  const isActive = stage === 'idle' || stage === 'liveness';

  const reset = useCallback(() => {
    setStage('idle');
    setMessage('');
    setProgress(0);
  }, []);

  const runAttendanceCheck = useCallback(async () => {
    if (!cameraRef.current) return;
    setStage('liveness');
    setMessage('Checking liveness...');

    let attempts = 0;
    const maxAttempts = 5;

    while (attempts < maxAttempts) {
      const base64 = await faceRecognitionService.captureVisionCameraFrame(cameraRef.current);
      if (!base64) {
        attempts++;
        await new Promise((r) => setTimeout(r, 300));
        continue;
      }

      const result = await faceRecognitionService.checkLivenessWithBase64(base64);
      if (!result) {
        attempts++;
        await new Promise((r) => setTimeout(r, 300));
        continue;
      }

      setProgress(result.progress);
      setMessage(result.message);

      if (result.verified) {
        setMessage('Liveness verified!');
        await new Promise((r) => setTimeout(r, 500));

        setStage('recognizing');
        setMessage('Recognizing face...');

        const recogResult = await faceRecognitionService.recognizeFaceWithBase64(base64);
        if (recogResult) {
          await faceRecognitionService.markAttendance(
            recogResult.person_id,
            recogResult.person_name,
            recogResult.confidence,
          );
          setStage('success');
          setMessage(`Welcome, ${recogResult.person_name}!`);
          setTimeout(reset, 3000);
        } else {
          setStage('failed');
          setMessage('Face not recognized');
          setTimeout(reset, 2000);
        }
        return;
      }

      if (result.progress >= 100 || result.progress < 0) {
        setStage('failed');
        setMessage('Liveness check failed');
        setTimeout(reset, 2000);
        return;
      }

      attempts++;
      await new Promise((r) => setTimeout(r, 500));
    }

    setStage('failed');
    setMessage('Liveness check timed out');
    setTimeout(reset, 2000);
  }, [reset]);

  if (!hasPermission) {
    return (
      <View style={styles.center}>
        <Text style={styles.permissionText}>We need camera permission for face recognition.</Text>
        <TouchableOpacity style={styles.button} onPress={requestPermission}>
          <Text style={styles.buttonText}>Grant Permission</Text>
        </TouchableOpacity>
      </View>
    );
  }

  if (!device) {
    return <View style={styles.center}><Text>No front camera available</Text></View>;
  }

  return (
    <View style={styles.container}>
      <Camera
        ref={cameraRef}
        style={StyleSheet.absoluteFill}
        device={device}
        isActive={isActive}
        photo={false}
      />

      <View style={styles.overlayContainer}>
        {stage === 'idle' && (
          <>
            <View style={styles.faceTarget} />
            <Text style={styles.hintText}>Align face here</Text>
            <TouchableOpacity style={styles.startButton} onPress={runAttendanceCheck}>
              <Text style={styles.startButtonText}>Mark Attendance</Text>
            </TouchableOpacity>
          </>
        )}

        {stage === 'liveness' && (
          <>
            <View style={styles.faceTarget} />
            <View style={styles.progressBar}>
              <View style={[styles.progressFill, { width: `${Math.max(0, Math.min(100, progress))}%` }]} />
            </View>
            <Text style={styles.statusText}>{message}</Text>
            <ActivityIndicator color="#fff" size="small" />
          </>
        )}

        {stage === 'recognizing' && (
          <>
            <ActivityIndicator color="#fff" size="large" />
            <Text style={styles.statusText}>{message}</Text>
          </>
        )}

        {stage === 'success' && (
          <View style={styles.successBox}>
            <Text style={styles.successIcon}>✓</Text>
            <Text style={styles.successText}>{message}</Text>
          </View>
        )}

        {stage === 'failed' && (
          <View style={styles.failureBox}>
            <Text style={styles.failureIcon}>✕</Text>
            <Text style={styles.failureText}>{message}</Text>
            <TouchableOpacity style={styles.retryButton} onPress={reset}>
              <Text style={styles.retryButtonText}>Retry</Text>
            </TouchableOpacity>
          </View>
        )}
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#000' },
  center: { flex: 1, justifyContent: 'center', alignItems: 'center', padding: 24 },
  permissionText: { textAlign: 'center', marginBottom: 16, fontSize: 16, color: '#333' },
  button: { backgroundColor: '#007AFF', paddingVertical: 12, paddingHorizontal: 24, borderRadius: 8 },
  buttonText: { color: '#fff', fontSize: 16, fontWeight: 'bold' },
  overlayContainer: {
    ...StyleSheet.absoluteFill,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: 'transparent',
  },
  faceTarget: {
    width: 250,
    height: 250,
    borderWidth: 2,
    borderColor: '#00E676',
    borderRadius: 125,
    backgroundColor: 'transparent',
  },
  hintText: {
    color: '#fff',
    marginTop: 20,
    fontSize: 14,
    fontWeight: '600',
    backgroundColor: 'rgba(0,0,0,0.6)',
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 12,
  },
  startButton: {
    marginTop: 40,
    backgroundColor: '#007AFF',
    paddingVertical: 14,
    paddingHorizontal: 32,
    borderRadius: 12,
  },
  startButtonText: { color: '#fff', fontSize: 18, fontWeight: 'bold' },
  progressBar: {
    width: 200,
    height: 6,
    backgroundColor: 'rgba(255,255,255,0.3)',
    borderRadius: 3,
    marginTop: 20,
    overflow: 'hidden',
  },
  progressFill: {
    height: '100%',
    backgroundColor: '#00E676',
    borderRadius: 3,
  },
  statusText: {
    color: '#fff',
    marginTop: 16,
    fontSize: 14,
    fontWeight: '600',
    textAlign: 'center',
    paddingHorizontal: 20,
  },
  successBox: { alignItems: 'center', backgroundColor: 'rgba(0,200,83,0.9)', padding: 32, borderRadius: 20 },
  successIcon: { fontSize: 48, color: '#fff', fontWeight: 'bold' },
  successText: { color: '#fff', fontSize: 18, fontWeight: '600', marginTop: 8 },
  failureBox: { alignItems: 'center', backgroundColor: 'rgba(255,68,68,0.9)', padding: 32, borderRadius: 20 },
  failureIcon: { fontSize: 48, color: '#fff', fontWeight: 'bold' },
  failureText: { color: '#fff', fontSize: 16, fontWeight: '600', marginTop: 8, textAlign: 'center' },
  retryButton: {
    marginTop: 16,
    backgroundColor: '#fff',
    paddingVertical: 10,
    paddingHorizontal: 24,
    borderRadius: 8,
  },
  retryButtonText: { color: '#FF4444', fontSize: 16, fontWeight: 'bold' },
});
