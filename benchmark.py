import cv2
import time
import numpy as np
from face_recognition_engine import FaceRecognitionEngine
from liveness_detection import LivenessDetector, LivenessState

class Benchmark:
    def __init__(self):
        print("Initializing benchmark...")
        self.face_engine = FaceRecognitionEngine()
        self.liveness    = LivenessDetector()
        self.results     = {
            "face_detection_ms":    [],
            "face_recognition_ms":  [],
            "liveness_ms":          [],
            "total_pipeline_ms":    [],
            "fps":                  []
        }
        print("✅ Benchmark ready!\n")

    def measure(self, label, func, *args):
        start  = time.perf_counter()
        result = func(*args)
        end    = time.perf_counter()
        ms     = (end - start) * 1000
        self.results[label].append(ms)
        return result, ms

    def run(self, frames=100):
        cap = cv2.VideoCapture(0)
        if not cap.isOpened():
            print("❌ Camera not available")
            return

        print(f"Running benchmark on {frames} frames...\n")
        self.liveness.start()

        frame_times = []
        count       = 0

        while count < frames:
            ret, frame = cap.read()
            if not ret:
                break

            frame = cv2.flip(frame, 1)
            frame_start = time.perf_counter()

            # 1. Liveness
            _, liveness_ms   = self.measure(
                "liveness_ms",
                self.liveness.process_frame,
                frame
            )

            # 2. Face detection + recognition
            _, recog_ms = self.measure(
                "face_recognition_ms",
                self.face_engine.recognize_face,
                frame
            )

            frame_end = time.perf_counter()
            total_ms  = (frame_end - frame_start) * 1000
            self.results["total_pipeline_ms"].append(total_ms)
            frame_times.append(frame_end)

            # FPS over last 30 frames
            if len(frame_times) > 30:
                frame_times.pop(0)
            if len(frame_times) > 1:
                fps = len(frame_times) / (frame_times[-1] - frame_times[0])
                self.results["fps"].append(fps)

            # Live display
            cv2.putText(frame, f"Liveness: {liveness_ms:.1f}ms", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
            cv2.putText(frame, f"Recognition: {recog_ms:.1f}ms", (10, 60),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
            cv2.putText(frame, f"Total: {total_ms:.1f}ms", (10, 90),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 0), 2)
            cv2.putText(frame, f"Frame: {count+1}/{frames}", (10, 120),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

            cv2.imshow('Benchmark', frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

            count += 1

        cap.release()
        cv2.destroyAllWindows()
        self.print_report()

    def avg(self, data):
        return sum(data) / len(data) if data else 0

    def print_report(self):
        print("\n" + "="*45)
        print("         BENCHMARK REPORT")
        print("="*45)
        print(f"{'Metric':<30} {'Avg':>8}ms")
        print("-"*45)
        print(f"{'Liveness Detection':<30} {self.avg(self.results['liveness_ms']):>8.2f}")
        print(f"{'Face Recognition':<30} {self.avg(self.results['face_recognition_ms']):>8.2f}")
        print(f"{'Total Pipeline':<30} {self.avg(self.results['total_pipeline_ms']):>8.2f}")
        print("-"*45)
        print(f"{'Average FPS':<30} {self.avg(self.results['fps']):>8.1f}")
        print("="*45)

        total_avg = self.avg(self.results['total_pipeline_ms'])
        print(f"\n{'✅ PASS' if total_avg < 1000 else '❌ FAIL'} — "
              f"Pipeline {'under' if total_avg < 1000 else 'over'} 1 second "
              f"({total_avg:.2f}ms)")
        print("="*45)


if __name__ == "__main__":
    b = Benchmark()
    b.run(frames=100)