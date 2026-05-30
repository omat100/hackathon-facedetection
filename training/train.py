import insightface
from insightface.app import FaceAnalysis

# Initialize with buffalo_sc - smallest and fastest model (~150MB)
app = FaceAnalysis(name='buffalo_sc', providers=['CPUExecutionProvider'])
app.prepare(ctx_id=0, det_size=(640, 640))

print("✅ InsightFace loaded successfully!")
print("Models:", [m for m in app.models])