import os
import cv2
import numpy as np
import mediapipe as mp
from pathlib import Path

# Paths
IMFDB_PATH = "../datasets/imfdb"
FAIRFACE_PATH = "../datasets/FairFace"
OUTPUT_PATH = "../datasets/processed"
IMG_SIZE = 112

# MediaPipe face detection
mp_face = mp.solutions.face_detection
detector = mp_face.FaceDetection(min_detection_confidence=0.5)

os.makedirs(OUTPUT_PATH, exist_ok=True)

def detect_and_crop(image):
    rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
    results = detector.process(rgb)
    if not results.detections:
        return None
    d = results.detections[0].location_data.relative_bounding_box
    h, w = image.shape[:2]
    x, y = int(d.xmin * w), int(d.ymin * h)
    bw, bh = int(d.width * w), int(d.height * h)
    # Add padding
    pad = 20
    x, y = max(0, x-pad), max(0, y-pad)
    bw, bh = min(w-x, bw+2*pad), min(h-y, bh+2*pad)
    face = image[y:y+bh, x:x+bw]
    return cv2.resize(face, (IMG_SIZE, IMG_SIZE))

def augment(image):
    augmented = [image]
    # Brightness variations
    augmented.append(cv2.convertScaleAbs(image, alpha=1.5, beta=30))  # harsh sunlight
    augmented.append(cv2.convertScaleAbs(image, alpha=0.5, beta=-30)) # low light
    # Shadow overlay
    shadow = image.copy()
    shadow[:, :IMG_SIZE//2] = (shadow[:, :IMG_SIZE//2] * 0.5).astype(np.uint8)
    augmented.append(shadow)
    # Flip
    augmented.append(cv2.flip(image, 1))
    return augmented

def process_dataset(dataset_path, dataset_name):
    print(f"Processing {dataset_name}...")
    count = 0
    for person_folder in Path(dataset_path).iterdir():
        if not person_folder.is_dir():
            continue
        out_folder = Path(OUTPUT_PATH) / dataset_name / person_folder.name
        out_folder.mkdir(parents=True, exist_ok=True)
        for img_path in person_folder.glob("*.jpg"):
            img = cv2.imread(str(img_path))
            if img is None:
                continue
            face = detect_and_crop(img)
            if face is None:
                continue
            for i, aug in enumerate(augment(face)):
                cv2.imwrite(str(out_folder / f"{img_path.stem}_aug{i}.jpg"), aug)
                count += 1
    print(f"{dataset_name} done — {count} images processed")

process_dataset(IMFDB_PATH, "imfdb")
process_dataset(FAIRFACE_PATH, "fairface")
print("All preprocessing complete!")
