import * as SQLite from 'expo-sqlite';
import { AttendanceRecord } from '../types';

export class DatabaseService {
  private db: SQLite.Database | null = null;

  async initialize() {
    try {
      this.db = await SQLite.openDatabaseAsync('attendance.db');
      await this.createTables();
      console.log('✅ Database initialized');
    } catch (error) {
      console.error('❌ Database initialization error:', error);
      throw error;
    }
  }

  private async createTables() {
    if (!this.db) throw new Error('Database not initialized');

    await this.db.execAsync(`
      CREATE TABLE IF NOT EXISTS attendance (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        person_id TEXT NOT NULL,
        timestamp TEXT NOT NULL,
        confidence REAL NOT NULL,
        synced INTEGER DEFAULT 0
      );
      
      CREATE TABLE IF NOT EXISTS persons (
        id TEXT PRIMARY KEY,
        name TEXT NOT NULL,
        embedding TEXT,
        last_sync TEXT
      );
      
      CREATE TABLE IF NOT EXISTS sync_log (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        batch_id TEXT,
        timestamp TEXT,
        record_count INTEGER,
        success INTEGER
      );
    `);
  }

  async logAttendance(personId: string, confidence: number): Promise<number> {
    if (!this.db) throw new Error('Database not initialized');

    const result = await this.db.runAsync(
      `INSERT INTO attendance (person_id, timestamp, confidence, synced)
       VALUES (?, ?, ?, 0)`,
      [personId, new Date().toISOString(), confidence]
    );

    console.log(`✅ Logged attendance for ${personId}`);
    return result.lastInsertRowId;
  }

  async getAttendanceRecords(limit: number = 100): Promise<AttendanceRecord[]> {
    if (!this.db) throw new Error('Database not initialized');

    const result = await this.db.getAllAsync<AttendanceRecord>(
      `SELECT * FROM attendance ORDER BY timestamp DESC LIMIT ?`,
      [limit]
    );

    return result.map(record => ({
      ...record,
      synced: Boolean(record.synced),
    }));
  }

  async getUnsyncedRecords(): Promise<AttendanceRecord[]> {
    if (!this.db) throw new Error('Database not initialized');

    const result = await this.db.getAllAsync<any>(
      `SELECT id, person_id, timestamp, confidence, synced 
       FROM attendance WHERE synced = 0`
    );

    return result.map(record => ({
      id: record.id,
      person_id: record.person_id,
      timestamp: record.timestamp,
      confidence: record.confidence,
      synced: false,
    }));
  }

  async markSynced(ids: number[]): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');

    for (const id of ids) {
      await this.db.runAsync(
        `UPDATE attendance SET synced = 1 WHERE id = ?`,
        [id]
      );
    }
  }

  async savePerson(id: string, name: string, embedding?: string): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');

    await this.db.runAsync(
      `INSERT OR REPLACE INTO persons (id, name, embedding, last_sync)
       VALUES (?, ?, ?, ?)`,
      [id, name, embedding || null, new Date().toISOString()]
    );
  }

  async getPersons(): Promise<Array<{ id: string; name: string }>> {
    if (!this.db) throw new Error('Database not initialized');

    const result = await this.db.getAllAsync<any>(
      `SELECT id, name FROM persons ORDER BY name ASC`
    );

    return result;
  }

  async getTodayAttendance(): Promise<Set<string>> {
    if (!this.db) throw new Error('Database not initialized');

    const today = new Date().toISOString().split('T')[0];
    const result = await this.db.getAllAsync<any>(
      `SELECT DISTINCT person_id FROM attendance 
       WHERE datetime(timestamp) >= ?`,
      [today]
    );

    return new Set(result.map(r => r.person_id));
  }

  async getStats(): Promise<{ total: number; synced: number; unsynced: number }> {
    if (!this.db) throw new Error('Database not initialized');

    const total = await this.db.getFirstAsync<{ count: number }>(
      `SELECT COUNT(*) as count FROM attendance`
    );
    const synced = await this.db.getFirstAsync<{ count: number }>(
      `SELECT COUNT(*) as count FROM attendance WHERE synced = 1`
    );
    const unsynced = await this.db.getFirstAsync<{ count: number }>(
      `SELECT COUNT(*) as count FROM attendance WHERE synced = 0`
    );

    return {
      total: total?.count || 0,
      synced: synced?.count || 0,
      unsynced: unsynced?.count || 0,
    };
  }

  async clearDatabase(): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');

    await this.db.execAsync(`
      DELETE FROM attendance;
      DELETE FROM persons;
      DELETE FROM sync_log;
    `);
  }
}

export const databaseService = new DatabaseService();
