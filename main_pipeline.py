import cv2
import numpy as np
import json
import os
from datetime import datetime
from enum import Enum
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision
from mediapipe import Image, ImageFormat
from face_recognition_engine import FaceRecognitionEngine
from liveness_detection import LivenessDetector, LivenessState

class PipelineState(Enum):
    IDLE = 0
    LIVENESS_CHECK = 1
    RECOGNIZING = 2
    SUCCESS = 3
    FAILED = -1

class AttendancePipeline:
    def __init__(self):
        print("Initializing pipeline...")
        self.face_engine = FaceRecognitionEngine()
        self.liveness = LivenessDetector()
        self.state = PipelineState.IDLE
        self.result = None
        self.result_timer = 0
        self.RESULT_DISPLAY_FRAMES = 90  # show result for ~3 seconds

        # Attendance log
        self.LOG_PATH = "attendance_log.json"
        self.attendance_log = self.load_log()
        print("✅ Pipeline ready!")

    def load_log(self):
        if os.path.exists(self.LOG_PATH):
            with open(self.LOG_PATH, 'r') as f:
                return json.load(f)
        return []

    def save_log(self):
        with open(self.LOG_PATH, 'w') as f:
            json.dump(self.attendance_log, f, indent=2)

    def log_attendance(self, person_id, score):
        entry = {
            "person_id": person_id,
            "timestamp": datetime.now().isoformat(),
            "confidence": round(score, 4),
            "synced": False  # AWS sync flag
        }
        self.attendance_log.append(entry)
        self.save_log()
        print(f"📝 Attendance logged: {person_id} at {entry['timestamp']}")

    def start(self):
        self.state = PipelineState.LIVENESS_CHECK
        self.liveness.start()
        self.result = None
        self.result_timer = 0

    def process_frame(self, frame):
        """
        Returns: (display_frame, status_message, pipeline_state)
        """
        display = frame.copy()

        if self.state == PipelineState.IDLE:
            self._draw_text(display, "Press SPACE to start", (30, 50), (255, 255, 255))
            return display, "idle", self.state

        elif self.state == PipelineState.LIVENESS_CHECK:
            liveness_state, message, landmarks = self.liveness.process_frame(frame)

            # Draw liveness UI
            color = (0, 165, 255)
            if liveness_state == LivenessState.FAILED:
                color = (0, 0, 255)
                self.state = PipelineState.FAILED
                self.result = "Liveness check failed!"
                self.result_timer = self.RESULT_DISPLAY_FRAMES
            elif liveness_state == LivenessState.VERIFIED:
                # Liveness passed → now recognize
                self.state = PipelineState.RECOGNIZING
                color = (0, 255, 0)

            self._draw_text(display, message, (30, 50), color)
            self._draw_text(display, f"Step: {liveness_state.name}", (30, 90), (255, 255, 255))
            self._draw_progress(display, liveness_state)

        elif self.state == PipelineState.RECOGNIZING:
            self._draw_text(display, "Recognizing...", (30, 50), (255, 255, 0))
            person_id, score, status = self.face_engine.recognize_face(frame)

            if person_id:
                self.log_attendance(person_id, score)
                self.result = f"Welcome, {person_id}! ({score:.0%})"
                self.state = PipelineState.SUCCESS
            else:
                self.result = f"Unknown person (score: {score:.2f})"
                self.state = PipelineState.FAILED

            self.result_timer = self.RESULT_DISPLAY_FRAMES

        elif self.state == PipelineState.SUCCESS:
            self._draw_text(display, self.result, (30, 50), (0, 255, 0))
            self._draw_text(display, "✅ Attendance logged!", (30, 90), (0, 255, 0))
            self.result_timer -= 1
            if self.result_timer <= 0:
                self.state = PipelineState.IDLE

        elif self.state == PipelineState.FAILED:
            self._draw_text(display, self.result or "Failed!", (30, 50), (0, 0, 255))
            self.result_timer -= 1
            if self.result_timer <= 0:
                self.state = PipelineState.IDLE

        return display, self.result, self.state

    def _draw_text(self, frame, text, pos, color):
        cv2.putText(frame, text, pos,
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, color, 2)

    def _draw_progress(self, frame, liveness_state):
        color = (0, 255, 0) if liveness_state == LivenessState.VERIFIED else \
            (0, 0, 255) if liveness_state == LivenessState.FAILED else \
                (0, 165, 255)
        cv2.putText(frame, f"Liveness: {liveness_state.name}", (30, 130),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)

if __name__ == "__main__":
    pipeline = AttendancePipeline()
    cap = cv2.VideoCapture(0)

    print("\n=== ATTENDANCE PIPELINE ===")
    print("SPACE → Start liveness check")
    print("R     → Register new face")
    print("Q     → Quit\n")

    register_mode = False
    register_name = ""

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break

        frame = cv2.flip(frame, 1)  # Mirror

        if register_mode:
            cv2.putText(frame, f"REGISTER MODE: {register_name}", (30, 50),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2)
            cv2.putText(frame, "Press ENTER to capture", (30, 90),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
        else:
            display, result, state = pipeline.process_frame(frame)
            frame = display

        cv2.imshow('Attendance System', frame)

        key = cv2.waitKey(1) & 0xFF

        if key == ord('q'):
            break
        elif key == ord(' ') and not register_mode:
            pipeline.start()
        elif key == ord('r') and not register_mode:
            register_name = input("Enter person name/ID: ")
            register_mode = True
        elif key == 13 and register_mode:  # ENTER
            success, msg = pipeline.face_engine.register_face(frame, register_name)
            print(msg)
            register_mode = False
            register_name = ""

    cap.release()
    cv2.destroyAllWindows()

    print(f"\n📋 Total attendance records: {len(pipeline.attendance_log)}")