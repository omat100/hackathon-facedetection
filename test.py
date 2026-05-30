import cv2
from face_recognition_engine import FaceRecognitionEngine

engine = FaceRecognitionEngine()

# Test with your webcam
cap = cv2.VideoCapture(0)
ret, frame = cap.read()
cap.release()

if ret:
    success, msg = engine.register_face(frame, "test_person")
    print(msg)
    match, score, status = engine.recognize_face(frame)
    print(f"Match: {match}, Score: {score:.2f}, Status: {status}")