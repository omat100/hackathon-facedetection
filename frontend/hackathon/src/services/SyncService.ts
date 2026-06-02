import { DatabaseService } from './DatabaseService';
import { networkService } from './NetworkService';

const API_BASE = 'http://localhost:5000';

class SyncService {
  private syncing = false;

  get isSyncing(): boolean {
    return this.syncing;
  }

  async fetchPersonDatabase(): Promise<void> {
    if (!networkService.isOnline) return;
    try {
      const resp = await fetch(`${API_BASE}/persons`);
      const json = await resp.json();
      if (json.status === 'success' && json.persons) {
        await DatabaseService.savePersons(json.persons);
      }
    } catch (e) {
      console.warn('Failed to fetch person database:', e);
    }
  }

  async syncAttendance(): Promise<{ success: boolean; count: number }> {
    if (this.syncing) return { success: false, count: 0 };
    if (!networkService.isOnline) return { success: false, count: 0 };

    this.syncing = true;
    try {
      const unsynced = await DatabaseService.getUnsynced();
      if (unsynced.length === 0) {
        return { success: true, count: 0 };
      }

      const batchId = `batch_${Date.now()}`;
      const batch = unsynced.slice(0, 50);

      const resp = await fetch(`${API_BASE}/sync-attendance`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          records: batch.map((r) => ({
            id: r.id,
            person_id: r.person_id,
            timestamp: r.timestamp,
            confidence: r.confidence,
          })),
          timestamp: new Date().toISOString(),
          client_info: { platform: 'react-native', version: '1.0.0' },
        }),
      });

      const json = await resp.json();
      if (json.status === 'success') {
        const ids = batch.map((r) => r.id);
        await DatabaseService.markSynced(ids);
        await DatabaseService.logSync(batchId, json.synced_count ?? batch.length, true);
        return { success: true, count: json.synced_count ?? batch.length };
      } else {
        await DatabaseService.logSync(batchId, 0, false);
        return { success: false, count: 0 };
      }
    } catch (e) {
      console.warn('Sync failed:', e);
      return { success: false, count: 0 };
    } finally {
      this.syncing = false;
    }
  }

  startAutoSync(intervalMs = 60000): () => void {
    const tick = async () => {
      if (networkService.isOnline && networkService.isIdle) {
        await this.fetchPersonDatabase();
        await this.syncAttendance();
      }
    };
    const interval = setInterval(tick, intervalMs);
    tick();
    return () => clearInterval(interval);
  }
}

export const syncService = new SyncService();
