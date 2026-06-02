import { CameraView } from 'expo-camera';
import { DatabaseService } from './DatabaseService';

const API_BASE = 'http://127.0.0.1:5000';

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

  async checkLiveness(sessionId?: string): Promise<LivenessResult | null> {
    return null;
  }

  async checkLivenessWithBase64(base64: string, sessionId?: string): Promise<LivenessResult | null> {
    try {
      const body: Record<string, string> = { frame_base64: base64 };
      if (sessionId) body.session_id = sessionId;

      const resp = await fetch(`${API_BASE}/liveness-check`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
      });
      const json = await resp.json();

      if (json.status === 'success') {
        return {
          verified: json.verified,
          confidence: json.confidence,
          message: json.message ?? '',
          session_id: json.session_id,
          progress: json.progress ?? 0,
        };
      }
      return null;
    } catch (e) {
      console.warn('Liveness check failed:', e);
      return null;
    }
  }

  async recognizeFaceWithBase64(base64: string): Promise<RecognitionResult | null> {
    try {
      const resp = await fetch(`${API_BASE}/recognize`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ frame_base64: base64 }),
      });
      const json = await resp.json();

      if (json.status === 'success') {
        return json.data as RecognitionResult;
      }
      return null;
    } catch (e) {
      console.warn('Face recognition failed:', e);
      return null;
    }
  }

  async markAttendance(personId: string, personName: string, confidence: number): Promise<void> {
    await DatabaseService.logAttendance(personId, personName, confidence);
  }
}

export const faceRecognitionService = new FaceRecognitionService();
