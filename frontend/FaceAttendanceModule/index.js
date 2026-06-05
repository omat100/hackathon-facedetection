import { NativeModules } from 'react-native';

const { FaceAttendanceModule } = NativeModules;

if (!FaceAttendanceModule) {
  console.warn('[FaceAttendanceModule] Native module not found. Rebuild the app.');
}

export default {
  /** Verify C++ engines initialized correctly */
  initialize: () =>
    FaceAttendanceModule.initializeModule(),

  /**
   * Process a base64 image frame.
   * Runs liveness check first, then face recognition if liveness passes.
   * @returns {{ liveness_verified, liveness_failed, liveness_message,
   *             person_id, confidence, recognized, recog_message }}
   */
  processBase64: (base64) =>
    FaceAttendanceModule.processBase64(base64),

  /** Process image from file path */
  processFaceImage: (imagePath) =>
    FaceAttendanceModule.processFaceImage(imagePath),

  /**
   * Register a face. Call multiple times with different angles for better accuracy.
   * Embeddings are averaged on repeat registration of same personId.
   * @returns {{ success: boolean, message: string }}
   */
  registerFace: (base64, personId) =>
    FaceAttendanceModule.registerFace(base64, personId),

  /** Reset liveness state machine — call before each new session */
  resetLiveness: () =>
    FaceAttendanceModule.resetLiveness(),

  /** Get attendance DB stats */
  getAttendanceStats: () =>
    FaceAttendanceModule.getAttendanceStats(),
};
