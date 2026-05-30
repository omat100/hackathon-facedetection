import cv2
import mediapipe as mp
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision
from mediapipe import Image, ImageFormat
import numpy as np
from enum import Enum
import urllib.request
import os

# Download model if not present
MODEL_PATH = "face_landmarker.task"
if not os.path.exists(MODEL_PATH):
    print("Downloading face landmarker model...")
    urllib.request.urlretrieve(
        "https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task",
        MODEL_PATH
    )
    print("✅ Model downloaded!")

class LivenessState(Enum):
    IDLE = 0
    BLINK_PENDING = 1
    SMILE_PENDING = 2
    TURN_PENDING = 3
    VERIFIED = 4
    FAILED = -1

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
        self.detector = vision.FaceLandmarker.create_from_options(options)
        self.state = LivenessState.IDLE
        self.blink_counter = 0
        self.state_frame_counter = 0
        self.MAX_FRAMES_PER_STATE = 150

        self.LEFT_EYE = [362, 385, 387, 263, 373, 380]
        self.RIGHT_EYE = [33, 160, 158, 133, 153, 144]
        self.MOUTH_CORNERS = [61, 291]
        self.LEFT_FACE = 234
        self.RIGHT_FACE = 454
        self.NOSE_TIP = 1

        print("✅ Liveness Detector ready!")

    def get_ear(self, landmarks, eye_indices, img_w, img_h):
        pts = np.array([
            (landmarks[i].x * img_w, landmarks[i].y * img_h)
            for i in eye_indices
        ])
        v1 = np.linalg.norm(pts[1] - pts[5])
        v2 = np.linalg.norm(pts[2] - pts[4])
        h = np.linalg.norm(pts[0] - pts[3])
        return (v1 + v2) / (2.0 * h) if h > 0 else 0

    def get_smile_ratio(self, landmarks, img_w, img_h):
        left_corner = landmarks[self.MOUTH_CORNERS[0]]
        right_corner = landmarks[self.MOUTH_CORNERS[1]]
        left_face = landmarks[self.LEFT_FACE]
        right_face = landmarks[self.RIGHT_FACE]
        mouth_width = abs(right_corner.x - left_corner.x) * img_w
        face_width = abs(right_face.x - left_face.x) * img_w
        return mouth_width / face_width if face_width > 0 else 0

    def get_head_turn_ratio(self, landmarks, img_w, img_h):
        nose = landmarks[self.NOSE_TIP]
        left = landmarks[self.LEFT_FACE]
        right = landmarks[self.RIGHT_FACE]
        nose_to_left = abs(nose.x - left.x)
        nose_to_right = abs(nose.x - right.x)
        total = nose_to_left + nose_to_right
        return nose_to_left / total if total > 0 else 0.5

    def reset(self):
        self.state = LivenessState.IDLE
        self.blink_counter = 0
        self.state_frame_counter = 0

    def start(self):
        self.reset()
        self.state = LivenessState.BLINK_PENDING
        print("Liveness started → Please BLINK")

    def process_frame(self, frame):
        img_h, img_w = frame.shape[:2]
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        mp_image = Image(image_format=ImageFormat.SRGB, data=rgb)
        results = self.detector.detect(mp_image)

        if not results.face_landmarks:
            return self.state, "No face detected", None

        landmarks = results.face_landmarks[0]
        self.state_frame_counter += 1

        if self.state_frame_counter > self.MAX_FRAMES_PER_STATE:
            self.state = LivenessState.FAILED
            return self.state, "Timeout! Try again.", landmarks

        avg_ear = (
            self.get_ear(landmarks, self.LEFT_EYE, img_w, img_h) +
            self.get_ear(landmarks, self.RIGHT_EYE, img_w, img_h)
        ) / 2.0
        smile_ratio = self.get_smile_ratio(landmarks, img_w, img_h)
        turn_ratio = self.get_head_turn_ratio(landmarks, img_w, img_h)

        if self.state == LivenessState.BLINK_PENDING:
            if avg_ear < 0.2:
                self.blink_counter += 1
            elif self.blink_counter >= 2:
                self.state = LivenessState.SMILE_PENDING
                self.state_frame_counter = 0
                self.blink_counter = 0
                print("✅ Blink detected → Please SMILE")
            return self.state, "Please BLINK", landmarks

        elif self.state == LivenessState.SMILE_PENDING:
            if smile_ratio > 0.48:
                self.state = LivenessState.TURN_PENDING
                self.state_frame_counter = 0
                print("✅ Smile detected → Please TURN YOUR HEAD")
            return self.state, "Please SMILE 😊", landmarks

        elif self.state == LivenessState.TURN_PENDING:
            if turn_ratio < 0.35 or turn_ratio > 0.65:
                self.state = LivenessState.VERIFIED
                print("✅ Head turn detected → LIVENESS VERIFIED!")
            return self.state, "Please TURN YOUR HEAD left or right", landmarks

        return self.state, "✅ Verified!", landmarks


if __name__ == "__main__":
    detector = LivenessDetector()
    detector.start()

    cap = cv2.VideoCapture(0)

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break

        state, message, landmarks = detector.process_frame(frame)

        color = (0, 255, 0) if state == LivenessState.VERIFIED else (0, 165, 255)
        if state == LivenessState.FAILED:
            color = (0, 0, 255)

        cv2.putText(frame, message, (30, 50),
                    cv2.FONT_HERSHEY_SIMPLEX, 1, color, 2)
        cv2.putText(frame, f"State: {state.name}", (30, 90),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

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