import cv2
import mediapipe as mp
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision
from mediapipe import Image, ImageFormat
import numpy as np
from enum import Enum
import os
import sys
import subprocess

MODEL_PATH = "face_landmarker.task"
if not os.path.exists(MODEL_PATH):
    print("Downloading face landmarker model...")
    os.system(
        "curl -k -o face_landmarker.task https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task"
    )
    print("✅ Model downloaded!")

class LivenessState(Enum):
    IDLE     =  0
    CHECKING =  1
    VERIFIED =  4
    FAILED   = -1

class LivenessDetector:
    def __init__(self):
        base_options = mp_python.BaseOptions(model_asset_path=MODEL_PATH)
        options = vision.FaceLandmarkerOptions(
            base_options=base_options,
            num_faces=1,
            min_face_detection_confidence=0.5,
            min_tracking_confidence=0.5,
            output_face_blendshapes=True
        )
        self.detector      = vision.FaceLandmarker.create_from_options(options)
        self.state         = LivenessState.IDLE
        self.frame_counter = 0
        self.MAX_FRAMES    = 150
        self.ear_history   = []
        self.EAR_WINDOW    = 5

        # Thresholds
        self.EAR_THRESH   = 0.20
        self.SMILE_THRESH = 0.48
        self.TURN_LOW     = 0.35
        self.TURN_HIGH    = 0.65

        # Landmark indices
        self.LEFT_EYE      = [362, 385, 387, 263, 373, 380]
        self.RIGHT_EYE     = [33, 160, 158, 133, 153, 144]
        self.MOUTH_CORNERS = [61, 291]
        self.LEFT_FACE     = 234
        self.RIGHT_FACE    = 454
        self.NOSE_TIP      = 1

        # Launch C++ engine as subprocess if available
        cpp_binary = os.path.join(os.path.dirname(__file__), 'cpp', 'liveness_engine')
        if os.path.exists(cpp_binary):
            self.cpp_process = subprocess.Popen(
                [cpp_binary],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                text=True
            )
            self.use_cpp = True
            print("✅ C++ engine loaded")
        else:
            self.cpp_process = None
            self.use_cpp     = False
            print("⚠️ C++ engine not found, using pure Python")

        print("✅ Liveness Detector ready!")

    def get_ear(self, landmarks, eye_indices, img_w, img_h):
        pts = np.array([
            (landmarks[i].x * img_w, landmarks[i].y * img_h)
            for i in eye_indices
        ])
        v1 = np.linalg.norm(pts[1] - pts[5])
        v2 = np.linalg.norm(pts[2] - pts[4])
        h  = np.linalg.norm(pts[0] - pts[3])
        return (v1 + v2) / (2.0 * h) if h > 0 else 0

    def get_smile_ratio(self, landmarks, img_w, img_h):
        left_corner  = landmarks[self.MOUTH_CORNERS[0]]
        right_corner = landmarks[self.MOUTH_CORNERS[1]]
        left_face    = landmarks[self.LEFT_FACE]
        right_face   = landmarks[self.RIGHT_FACE]
        mouth_width  = abs(right_corner.x - left_corner.x) * img_w
        face_width   = abs(right_face.x - left_face.x) * img_w
        return mouth_width / face_width if face_width > 0 else 0

    def get_head_turn_ratio(self, landmarks, img_w, img_h):
        nose     = landmarks[self.NOSE_TIP]
        left     = landmarks[self.LEFT_FACE]
        right    = landmarks[self.RIGHT_FACE]
        to_left  = abs(nose.x - left.x)
        to_right = abs(nose.x - right.x)
        total    = to_left + to_right
        return to_left / total if total > 0 else 0.5

    def _restart_cpp(self):
        cpp_binary = os.path.join(os.path.dirname(__file__), 'cpp', 'liveness_engine')
        if self.cpp_process:
            self.cpp_process.terminate()
        self.cpp_process = subprocess.Popen(
            [cpp_binary],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True
        )

    def reset(self):
        self.state         = LivenessState.IDLE
        self.frame_counter = 0
        self.ear_history   = []
        if self.use_cpp:
            self._restart_cpp()

    def start(self):
        self.reset()
        self.state = LivenessState.CHECKING

    def process_frame(self, frame):
        frame = self.apply_clahe(frame)
        img_h, img_w = frame.shape[:2]
        rgb      = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        mp_image = Image(image_format=ImageFormat.SRGB, data=rgb)
        results  = self.detector.detect(mp_image)

        if not results.face_landmarks:
            return self.state, "No face detected", None

        if self.state != LivenessState.CHECKING:
            msg = "✅ Alive!" if self.state == LivenessState.VERIFIED else "❌ Spoof detected!"
            return self.state, msg, None

        landmarks = results.face_landmarks[0]
        self.frame_counter += 1

        if self.frame_counter > self.MAX_FRAMES:
            self.state = LivenessState.FAILED
            return self.state, "❌ No movement — possible spoof!", None

        # Python computes real metrics from MediaPipe landmarks
        avg_ear = (
            self.get_ear(landmarks, self.LEFT_EYE, img_w, img_h) +
            self.get_ear(landmarks, self.RIGHT_EYE, img_w, img_h)
        ) / 2.0
        smile_ratio = self.get_smile_ratio(landmarks, img_w, img_h)
        turn_ratio  = self.get_head_turn_ratio(landmarks, img_w, img_h)

        if self.use_cpp:
            # Feed real metrics to C++ engine
            self.cpp_process.stdin.write(
                f"{avg_ear:.4f} {smile_ratio:.4f} {turn_ratio:.4f}\n"
            )
            self.cpp_process.stdin.flush()
            result = self.cpp_process.stdout.readline().strip()

            if result.startswith("VERIFIED"):
                action     = result.split()[1]
                self.state = LivenessState.VERIFIED
                return self.state, f"✅ Alive! ({action})", landmarks
            elif result.startswith("FAILED"):
                self.state = LivenessState.FAILED
                return self.state, "❌ No movement — possible spoof!", landmarks

        else:
            # Pure Python fallback
            self.ear_history.append(avg_ear)
            if len(self.ear_history) > self.EAR_WINDOW:
                self.ear_history.pop(0)

            blink_detected = (
                len(self.ear_history) == self.EAR_WINDOW and
                all(e < self.EAR_THRESH for e in self.ear_history)
            )
            smile_detected = smile_ratio > self.SMILE_THRESH
            turn_detected  = turn_ratio < self.TURN_LOW or turn_ratio > self.TURN_HIGH

            if blink_detected or smile_detected or turn_detected:
                self.state = LivenessState.VERIFIED
                action = "BLINK" if blink_detected else "SMILE" if smile_detected else "TURN"
                return self.state, f"✅ Alive! ({action})", landmarks

        return self.state, "Verifying...", landmarks

    def apply_clahe(self, frame):
        lab = cv2.cvtColor(frame, cv2.COLOR_BGR2Lab)
        channels = list(cv2.split(lab))
        clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
        channels[0] = clahe.apply(channels[0])
        lab = cv2.merge(channels)
        return cv2.cvtColor(lab, cv2.COLOR_Lab2BGR)

    def __del__(self):
        if self.use_cpp and self.cpp_process:
            self.cpp_process.terminate()


if __name__ == "__main__":
    detector = LivenessDetector()
    detector.start()
    cap = cv2.VideoCapture(0)

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break

        frame = cv2.flip(frame, 1)
        state, message, landmarks = detector.process_frame(frame)

        color = (0, 255, 0)   if state == LivenessState.VERIFIED else \
                (0, 0, 255)   if state == LivenessState.FAILED   else \
                (0, 165, 255)

        cv2.putText(frame, message, (30, 50),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, color, 2)
        cv2.putText(frame, f"State: {state.name}", (30, 90),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)

        cv2.imshow('Liveness Detection', frame)

        if state == LivenessState.VERIFIED:
            cv2.waitKey(2000)
            break
        if state == LivenessState.FAILED:
            detector.reset()
            detector.start()
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()