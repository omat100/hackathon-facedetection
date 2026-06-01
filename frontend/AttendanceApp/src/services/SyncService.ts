import { AttendanceRecord } from '../types';

export interface SyncConfig {
  backendUrl: string;
  batchSize: number;
  retryAttempts: number;
}

export class SyncService {
  private config: SyncConfig = {
    backendUrl: 'http://localhost:5000',
    batchSize: 50,
    retryAttempts: 3,
  };

  async syncAttendance(records: AttendanceRecord[]): Promise<{ success: boolean; synced: number }> {
    if (records.length === 0) {
      return { success: true, synced: 0 };
    }

    let syncedCount = 0;

    for (let i = 0; i < records.length; i += this.config.batchSize) {
      const batch = records.slice(i, i + this.config.batchSize);

      try {
        const success = await this.sendBatch(batch);
        if (success) {
          syncedCount += batch.length;
        }
      } catch (error) {
        console.error('Batch sync failed:', error);
      }
    }

    return {
      success: syncedCount === records.length,
      synced: syncedCount,
    };
  }

  private async sendBatch(batch: AttendanceRecord[]): Promise<boolean> {
    try {
      const payload = {
        records: batch,
        timestamp: new Date().toISOString(),
        client_info: {
          platform: 'react-native',
          version: '1.0.0',
        },
      };

      const response = await fetch(`${this.config.backendUrl}/sync-attendance`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(payload),
      });

      return response.ok;
    } catch (error) {
      console.error('Batch send failed:', error);
      return false;
    }
  }

  async fetchPersonDatabase(): Promise<Array<{ id: string; name: string }>> {
    try {
      const response = await fetch(`${this.config.backendUrl}/persons`);

      if (!response.ok) {
        return [];
      }

      const data = await response.json();
      return data.persons || [];
    } catch (error) {
      console.error('Fetch persons failed:', error);
      return [];
    }
  }

  setConfig(config: Partial<SyncConfig>) {
    this.config = { ...this.config, ...config };
  }

  getConfig(): SyncConfig {
    return this.config;
  }
}

export const syncService = new SyncService();
