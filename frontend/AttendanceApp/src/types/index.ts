export interface AttendanceRecord {
  id?: number;
  person_id: string;
  timestamp: string;
  confidence: number;
  synced: boolean;
}

export interface Person {
  id: string;
  name: string;
  embedding?: number[];
  lastAttendance?: string;
}

export interface CameraFrameData {
  width: number;
  height: number;
  isFrontal: boolean;
}

export interface LivenessCheckResult {
  verified: boolean;
  confidence: number;
}

export interface FaceRecognitionResult {
  personId: string;
  personName: string;
  confidence: number;
  livenessVerified: boolean;
}

export interface SyncPayload {
  attendanceRecords: AttendanceRecord[];
  timestamp: string;
}

export enum AppState {
  IDLE = 'IDLE',
  LIVENESS_CHECK = 'LIVENESS_CHECK',
  FACE_DETECTION = 'FACE_DETECTION',
  SUCCESS = 'SUCCESS',
  FAILED = 'FAILED',
  OFFLINE = 'OFFLINE',
}
