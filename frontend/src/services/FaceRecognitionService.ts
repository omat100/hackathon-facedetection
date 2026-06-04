import { Camera } from 'react-native-vision-camera';
import { NativeModules } from 'react-native';
import * as FileSystem from 'expo-file-system';
import { DatabaseService } from './DatabaseService';

const { FaceAttendanceModule } = NativeModules;

export interface RecognitionResult {
  person_id: string;
  person_name: string;
  confidence: number;
  liveness_verified: boolean;
}

export interface LivenessResult {
  verified: boolean;
  confidence: number;
  message: string;
  session_id: string;
  progress: number;
}

class FaceRecognitionService {

  async captureVisionCameraFrame(camera: Camera): Promise<string | null> {
    try {
      const snapshot = await camera.takeSnapshot({
        quality: 50,
      });
      const base64 = await FileSystem.readAsStringAsync(snapshot.path, {
        encoding: FileSystem.EncodingType.Base64,
      });
      return base64;
    } catch (e) {
      console.warn('Failed to capture frame:', e);
      return null;
    }
  }

  async checkLivenessWithBase64(
    base64: string,
    _sessionId?: string
  ): Promise<LivenessResult | null> {
    if (!FaceAttendanceModule) {
      console.warn('[FaceRecognitionService] FaceAttendanceModule not available');
      return null;
    }

    try {
      const result = await FaceAttendanceModule.processBase64(base64);

      return {
        verified:   result.liveness_verified,
        confidence: result.confidence ?? 0,
        message:    result.liveness_message ?? '',
        session_id: '',
        progress:   result.liveness_verified ? 1 : result.liveness_failed ? -1 : 0.5,
      };
    } catch (e) {
      console.warn('[FaceRecognitionService] Liveness check failed:', e);
      return null;
    }
  }

  async recognizeFaceWithBase64(
    base64: string
  ): Promise<RecognitionResult | null> {
    if (!FaceAttendanceModule) {
      console.warn('[FaceRecognitionService] FaceAttendanceModule not available');
      return null;
    }

    try {
      const result = await FaceAttendanceModule.processBase64(base64);

      if (!result.recognized || !result.person_id) {
        return null;
      }

      return {
        person_id:        result.person_id,
        person_name:      result.person_id,
        confidence:       result.confidence,
        liveness_verified: result.liveness_verified,
      };
    } catch (e) {
      console.warn('[FaceRecognitionService] Recognition failed:', e);
      return null;
    }
  }

  async registerFace(
    base64: string,
    personId: string
  ): Promise<{ success: boolean; message: string }> {
    if (!FaceAttendanceModule) {
      return { success: false, message: 'Native module not available' };
    }

    try {
      const result = await FaceAttendanceModule.registerFace(base64, personId);
      return result;
    } catch (e: any) {
      console.warn('[FaceRecognitionService] Register failed:', e);
      return { success: false, message: e?.message ?? 'Unknown error' };
    }
  }

  async resetLiveness(): Promise<void> {
    if (!FaceAttendanceModule) return;
    try {
      await FaceAttendanceModule.resetLiveness();
    } catch (e) {
      console.warn('[FaceRecognitionService] Reset liveness failed:', e);
    }
  }

  async markAttendance(
    personId: string,
    personName: string,
    confidence: number
  ): Promise<void> {
    await DatabaseService.logAttendance(personId, personName, confidence);
  }
}

export const faceRecognitionService = new FaceRecognitionService();
