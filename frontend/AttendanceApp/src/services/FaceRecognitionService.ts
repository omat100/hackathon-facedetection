import { FaceRecognitionResult, LivenessCheckResult } from '../types';

export interface BackendResponse {
  status: 'success' | 'failed';
  message: string;
  data?: {
    person_id: string;
    person_name: string;
    confidence: number;
    liveness_verified: boolean;
  };
}

export class FaceRecognitionService {
  private backendUrl: string = 'http://localhost:5000'; // Configure as needed
  private useOfflineMode: boolean = true;
  private mockDatabase: Map<string, string> = new Map();

  constructor() {
    this.setupMockDatabase();
  }

  private setupMockDatabase() {
    // Mock employee database for offline mode
    this.mockDatabase.set('emp001', 'John Doe');
    this.mockDatabase.set('emp002', 'Jane Smith');
    this.mockDatabase.set('emp003', 'Mike Johnson');
    this.mockDatabase.set('emp004', 'Sarah Williams');
    this.mockDatabase.set('emp005', 'Om Thakur');
  }

  async recognizeFace(frameBuffer: any): Promise<FaceRecognitionResult | null> {
    try {
      if (this.useOfflineMode) {
        // In offline mode, return mock result
        return this.getMockResult();
      }

      // Send to backend for recognition
      const formData = new FormData();
      formData.append('frame', frameBuffer);

      const response = await fetch(`${this.backendUrl}/recognize`, {
        method: 'POST',
        body: formData,
      });

      if (!response.ok) return null;

      const data: BackendResponse = await response.json();

      if (data.status === 'success' && data.data) {
        return {
          personId: data.data.person_id,
          personName: data.data.person_name,
          confidence: data.data.confidence,
          livenessVerified: data.data.liveness_verified,
        };
      }

      return null;
    } catch (error) {
      console.warn('Face recognition error:', error);
      return null;
    }
  }

  async checkLiveness(frameBuffer: any): Promise<LivenessCheckResult> {
    try {
      if (this.useOfflineMode) {
        // Simulate liveness check with random confidence
        return {
          verified: Math.random() > 0.3,
          confidence: Math.random() * 0.4 + 0.6, // 0.6-1.0
        };
      }

      const formData = new FormData();
      formData.append('frame', frameBuffer);

      const response = await fetch(`${this.backendUrl}/liveness-check`, {
        method: 'POST',
        body: formData,
      });

      if (!response.ok) {
        return { verified: false, confidence: 0 };
      }

      const data = await response.json();
      return {
        verified: data.verified,
        confidence: data.confidence,
      };
    } catch (error) {
      console.warn('Liveness check error:', error);
      return { verified: false, confidence: 0 };
    }
  }

  private getMockResult(): FaceRecognitionResult {
    const employees = Array.from(this.mockDatabase.entries());
    const randomIndex = Math.floor(Math.random() * employees.length);
    const [personId, personName] = employees[randomIndex];

    return {
      personId,
      personName,
      confidence: Math.random() * 0.2 + 0.8, // 0.8-1.0
      livenessVerified: Math.random() > 0.2,
    };
  }

  setBackendUrl(url: string) {
    this.backendUrl = url;
  }

  setOfflineMode(offline: boolean) {
    this.useOfflineMode = offline;
  }

  addToMockDatabase(id: string, name: string) {
    this.mockDatabase.set(id, name);
  }

  getMockDatabase() {
    return Array.from(this.mockDatabase.entries()).map(([id, name]) => ({
      id,
      name,
    }));
  }
}

export const faceRecognitionService = new FaceRecognitionService();
