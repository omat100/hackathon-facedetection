import insightface
from insightface.app import FaceAnalysis
import numpy as np
import json
import os

class FaceRecognitionEngine:
    def __init__(self):
        self.app = FaceAnalysis(name='buffalo_sc',
                                providers=['CPUExecutionProvider'])
        self.app.prepare(ctx_id=0, det_size=(640, 640))
        self.database = {}  # {person_id: embedding}
        self.DB_PATH = "face_database.json"
        self.load_database()
        print("✅ Face Recognition Engine ready!")

    def register_face(self, image, person_id):
        """Register a new person's face"""
        faces = self.app.get(image)
        if not faces:
            return False, "No face detected"
        embedding = faces[0].embedding
        self.database[person_id] = embedding.tolist()
        self.save_database()
        return True, f"Registered {person_id} successfully"

    def recognize_face(self, image, threshold=0.4):
        """Recognize a face from the database"""
        faces = self.app.get(image)
        if not faces:
            return None, 0.0, "No face detected"
        embedding = faces[0].embedding
        best_match = None
        best_score = -1
        for person_id, stored_embedding in self.database.items():
            stored = np.array(stored_embedding)
            # Cosine similarity
            score = np.dot(embedding, stored) / (
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