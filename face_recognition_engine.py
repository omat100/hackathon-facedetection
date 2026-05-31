import insightface
from insightface.app import FaceAnalysis
import numpy as np
import json
import os
import subprocess

class FaceRecognitionEngine:
    def __init__(self):
        self.app = FaceAnalysis(name='buffalo_sc',
                                providers=['CPUExecutionProvider'])
        self.app.prepare(ctx_id=0, det_size=(640, 640))
        self.database = {}
        self.DB_PATH  = "face_database.json"
        self.load_database()

        # Launch C++ face selector
        cpp_binary = os.path.join(os.path.dirname(__file__), 'cpp', 'liveness_engine')
        if os.path.exists(cpp_binary):
            self.cpp_selector = subprocess.Popen(
                [cpp_binary, 'face_select'],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                text=True
            )
            self.use_cpp_selector = True
            print("✅ C++ face selector loaded")
        else:
            self.cpp_selector     = None
            self.use_cpp_selector = False

        print("✅ Face Recognition Engine ready!")

    def _select_best_face(self, faces):
        if len(faces) == 1:
            return faces[0]

        if self.use_cpp_selector:
            boxes = []
            for f in faces:
                b = f.bbox
                boxes.append(f"{b[0]:.2f} {b[1]:.2f} {b[2]:.2f} {b[3]:.2f}")
            line = f"{len(faces)} " + " ".join(boxes) + "\n"
            self.cpp_selector.stdin.write(line)
            self.cpp_selector.stdin.flush()
            idx = int(self.cpp_selector.stdout.readline().strip())
            return faces[idx] if 0 <= idx < len(faces) else faces[0]

        return max(faces, key=lambda f: (
            abs(f.bbox[2] - f.bbox[0]) * abs(f.bbox[3] - f.bbox[1])
        ))

    def register_face(self, image, person_id):
        faces = self.app.get(image)
        if not faces:
            return False, "No face detected"
        face      = self._select_best_face(faces)
        embedding = face.embedding
        self.database[person_id] = embedding.tolist()
        self.save_database()
        return True, f"Registered {person_id} successfully"

    def recognize_face(self, image, threshold=0.4):
        faces = self.app.get(image)
        if not faces:
            return None, 0.0, "No face detected"
        face      = self._select_best_face(faces)
        embedding = face.embedding

        best_match = None
        best_score = -1
        for person_id, stored_embedding in self.database.items():
            stored = np.array(stored_embedding)
            score  = np.dot(embedding, stored) / (
                np.linalg.norm(embedding) * np.linalg.norm(stored)
            )
            if score > best_score:
                best_score = score
                best_match = person_id

        if best_score >= threshold:
            return best_match, float(best_score), "Match found"
        return None, float(best_score), "No match found"

    def save_database(self):
        with open(self.DB_PATH, 'w') as f:
            json.dump(self.database, f)

    def load_database(self):
        if os.path.exists(self.DB_PATH):
            with open(self.DB_PATH, 'r') as f:
                self.database = json.load(f)
            print(f"Loaded {len(self.database)} registered faces")

    def __del__(self):
        if hasattr(self, 'cpp_selector') and self.cpp_selector:
            self.cpp_selector.terminate()