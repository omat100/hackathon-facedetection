import { CameraView } from 'expo-camera';
import { NativeModules } from 'react-native';
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
  // ─── Capture ────────────────────────────────────────────────────────────────

  async captureFrameBase64(camera: CameraView): Promise<string | null> {
    try {
      const photo = await camera.takePictureAsync({
        base64: true,
        quality: 0.5,
      });
      return photo?.base64 ?? null;
    } catch (e) {
      console.warn('Failed to capture frame:', e);
      return null;
    }
  }

  // ─── Liveness (legacy stub — kept for API compatibility) ─────────────────

  async checkLiveness(sessionId?: string): Promise<LivenessResult | null> {
    return null;
  }

  // ─── Liveness + Recognition via Native Module ────────────────────────────

  async checkLivenessWithBase64(
    base64: string,
    sessionId?: string
  ): Promise<LivenessResult | null> {
    if (!FaceAttendanceModule) {
      console.warn('[FaceRecognitionService] FaceAttendanceModule not available');
      return null;
    }

    try {
      const result = await FaceAttendanceModule.processBase64(base64);

      // result shape from native:
      // {
      //   liveness_verified: boolean,
      //   liveness_failed:   boolean,
      //   liveness_message:  string,
      //   person_id:         string,
      //   confidence:        number,
      //   recognized:        boolean,
      // }

      return {
        verified:   result.liveness_verified,
        confidence: result.confidence ?? 0,
        message:    result.liveness_message ?? '',
        session_id: sessionId ?? '',
        progress:   result.liveness_verified ? 1 : result.liveness_failed ? -1 : 0.5,
      };
    } catch (e) {
      console.warn('[FaceRecognitionService] Liveness check failed:', e);
      return null;
    }
  }

  // ─── Recognition via Native Module ───────────────────────────────────────

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
        person_name:      result.person_id, // use person_id as name unless you have a lookup
        confidence:       result.confidence,
        liveness_verified: result.liveness_verified,
      };
    } catch (e) {
      console.warn('[FaceRecognitionService] Recognition failed:', e);
      return null;
    }
  }

  // ─── Register a face ─────────────────────────────────────────────────────

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

  // ─── Reset liveness state machine ────────────────────────────────────────

  async resetLiveness(): Promise<void> {
    if (!FaceAttendanceModule) return;
    try {
      await FaceAttendanceModule.resetLiveness();
    } catch (e) {
      console.warn('[FaceRecognitionService] Reset liveness failed:', e);
    }
  }

  // ─── Attendance ──────────────────────────────────────────────────────────

  async markAttendance(
    personId: string,
    personName: string,
    confidence: number
  ): Promise<void> {
    await DatabaseService.logAttendance(personId, personName, confidence);
  }
}

export const faceRecognitionService = new FaceRecognitionService();