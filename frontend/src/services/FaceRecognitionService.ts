import { Camera } from 'react-native-vision-camera';
import { NativeModules } from 'react-native';
import * as FileSystem from 'expo-file-system';
import { DatabaseService } from './DatabaseService';
import { CameraView } from 'expo-camera';

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
  recognized: boolean;
  person_id: string;
}

class FaceRecognitionService {

  // ─── Capture ──────────────────────────────────────────────────────────────

  async captureVisionCameraFrame(camera: Camera | CameraView): Promise<string | null> {
  try {
    // react-native-vision-camera
    if ('takePhoto' in camera) {
      const photo = await (camera as Camera).takePhoto({ flash: 'off' });
      const base64 = await FileSystem.readAsStringAsync(`file://${photo.path}`, {
        encoding: FileSystem.EncodingType.Base64,
      });
      return base64;
    }
    // expo-camera
    if ('takePictureAsync' in camera) {
      const photo = await (camera as CameraView).takePictureAsync({ base64: true, quality: 0.5 });
      return photo?.base64 ?? null;
    }
    return null;
  } catch (e) {
    console.warn('[FaceService] Capture failed:', e);
    return null;
  }
}

  // ─── Liveness check (legacy stub) ─────────────────────────────────────────

  async checkLiveness(_sessionId?: string): Promise<LivenessResult | null> {
    return null;
  }

  // ─── Main: liveness + recognition in one native call ──────────────────────

  async checkLivenessWithBase64(
    base64: string,
    _sessionId?: string
  ): Promise<LivenessResult | null> {
    if (!FaceAttendanceModule) {
      console.warn('[FaceService] Native module not available');
      return null;
    }
    try {
      const r = await FaceAttendanceModule.processBase64(base64);
      console.log('[FaceService] Native result:', JSON.stringify(r));
      return {
        verified:   r.liveness_verified,
        confidence: r.confidence ?? 0,
        message:    r.liveness_message ?? '',
        session_id: _sessionId ?? '',
        progress:   r.liveness_verified ? 1 : r.liveness_failed ? -1 : 0.5,
        recognized: r.recognized,
        person_id:  r.person_id ?? '',
      };
    } catch (e) {
      console.warn('[FaceService] Liveness failed:', e);
      return null;
    }
  }

  // ─── Recognition only ─────────────────────────────────────────────────────

  async recognizeFaceWithBase64(base64: string): Promise<RecognitionResult | null> {
    if (!FaceAttendanceModule) return null;
    try {
      const r = await FaceAttendanceModule.processBase64(base64);
      console.log('[FaceService] Recog result:', JSON.stringify(r));
      if (!r.recognized || !r.person_id) return null;
      return {
        person_id:         r.person_id,
        person_name:       r.person_id,
        confidence:        r.confidence,
        liveness_verified: r.liveness_verified,
      };
    } catch (e) {
      console.warn('[FaceService] Recognition failed:', e);
      return null;
    }
  }

  // ─── Register ─────────────────────────────────────────────────────────────

  async registerFace(
    base64: string,
    personId: string
  ): Promise<{ success: boolean; message: string }> {
    if (!FaceAttendanceModule) return { success: false, message: 'Module not available' };
    try {
      return await FaceAttendanceModule.registerFace(base64, personId);
    } catch (e: any) {
      return { success: false, message: e?.message ?? 'Unknown error' };
    }
  }

  // ─── Reset liveness ───────────────────────────────────────────────────────

  async resetLiveness(): Promise<void> {
    if (!FaceAttendanceModule) return;
    try { 
      await new Promise((r) => setTimeout(r, 1000));
      await FaceAttendanceModule.resetLiveness();
      console.log('[FaceService] Liveness reset complete');
    }
    catch (e) { console.warn('[FaceService] Reset failed:', e); }
  }

  // ─── Attendance ───────────────────────────────────────────────────────────

  async markAttendance(personId: string, personName: string, confidence: number): Promise<void> {
    await DatabaseService.logAttendance(personId, personName, confidence);
  }
}

export const faceRecognitionService = new FaceRecognitionService();
