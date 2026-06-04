import { NativeModules } from 'react-native';

const { FaceAttendanceModule } = NativeModules;

if (!FaceAttendanceModule) {
  console.warn(
    '[FaceAttendanceModule] Native module not found. ' +
    'Make sure the app was rebuilt after adding the module.'
  );
}

// FIX: strip "data:image/...;base64," prefix that camera libraries often add
function stripDataUri(base64) {
  if (typeof base64 === 'string' && base64.startsWith('data:')) {
    const idx = base64.indexOf(',');
    if (idx !== -1) return base64.substring(idx + 1);
  }
  return base64;
}

export default {
  /**
   * Initialize and verify the C++ engines are ready.
   * Call this once on app start before using any other method.
   * @returns {Promise<boolean>}
   */
  initialize: () =>
    FaceAttendanceModule.initializeModule(),

  /**
   * Process a base64-encoded JPEG/PNG frame.
   * Runs liveness check and, if verified, face recognition.
   * Accepts both raw base64 and data URIs (data:image/jpeg;base64,...).
   * @param {string} base64 - base64 image string
   * @returns {Promise<{
   *   liveness_verified: boolean,
   *   liveness_failed: boolean,
   *   liveness_message: string,
   *   person_id: string,
   *   confidence: number,
   *   recognized: boolean
   * }>}
   */
  processBase64: (base64) =>
    FaceAttendanceModule.processBase64(stripDataUri(base64)),

  /**
   * Process an image from a file path.
   * @param {string} imagePath - absolute path to image file
   * @returns {Promise<same as processBase64>}
   */
  processFaceImage: (imagePath) =>
    FaceAttendanceModule.processFaceImage(imagePath),

  /**
   * Register a face for a given person ID.
   * Accepts both raw base64 and data URIs.
   * @param {string} base64 - base64 image string
   * @param {string} personId - unique identifier for this person
   * @returns {Promise<{ success: boolean, message: string }>}
   */
  registerFace: (base64, personId) =>
    FaceAttendanceModule.registerFace(stripDataUri(base64), personId),

  /**
   * Reset the liveness detector state machine.
   * Call this before starting a new liveness check session.
   * Note: After a successful recognition, liveness resets automatically.
   * @returns {Promise<boolean>}
   */
  resetLiveness: () =>
    FaceAttendanceModule.resetLiveness(),
};
