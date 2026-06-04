import * as SQLite from 'expo-sqlite';

export interface AttendanceRecord {
  id: number;
  person_id: string;
  person_name: string;
  timestamp: string;
  confidence: number;
  synced: number;
}

export interface Person {
  id: string;
  name: string;
  embedding?: string;
  last_sync?: string;
}

export interface SyncLog {
  id: number;
  batch_id: string;
  timestamp: string;
  record_count: number;
  success: number;
}

let db: SQLite.SQLiteDatabase | null = null;

async function getDb(): Promise<SQLite.SQLiteDatabase> {
  if (!db) {
    try {
      db = await SQLite.openDatabaseAsync('attendance.db');
      await initTables();
    } catch (e) {
      console.error('Database init failed:', e);
      throw e;
    }
  }
  return db;
}

async function initTables(): Promise<void> {
  const database = db!;
  await database.execAsync(`
    CREATE TABLE IF NOT EXISTS attendance (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      person_id TEXT NOT NULL,
      person_name TEXT NOT NULL DEFAULT '',
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

async function withDb<T>(fn: (db: SQLite.SQLiteDatabase) => Promise<T>): Promise<T> {
  try {
    const database = await getDb();
    return await fn(database);
  } catch (e) {
    console.error('Database operation failed:', e);
    throw e;
  }
}

export const DatabaseService = {
  async logAttendance(personId: string, personName: string, confidence: number): Promise<void> {
    return withDb(async (database) => {
      await database.runAsync(
        'INSERT INTO attendance (person_id, person_name, timestamp, confidence, synced) VALUES (?, ?, ?, ?, 0)',
        personId,
        personName,
        new Date().toISOString(),
        Math.round(confidence * 10000) / 10000,
      );
    });
  },

  async getAttendance(limit = 50): Promise<AttendanceRecord[]> {
    return withDb(async (database) => {
      return await database.getAllAsync<AttendanceRecord>(
        'SELECT * FROM attendance ORDER BY timestamp DESC LIMIT ?',
        limit,
      );
    });
  },

  async getUnsynced(): Promise<AttendanceRecord[]> {
    return withDb(async (database) => {
      return await database.getAllAsync<AttendanceRecord>(
        'SELECT * FROM attendance WHERE synced = 0 ORDER BY timestamp ASC',
      );
    });
  },

  async markSynced(ids: number[]): Promise<void> {
    if (ids.length === 0) return;
    return withDb(async (database) => {
      const placeholders = ids.map(() => '?').join(',');
      await database.runAsync(
        `UPDATE attendance SET synced = 1 WHERE id IN (${placeholders})`,
        ...ids,
      );
    });
  },

  async purgeSynced(): Promise<number> {
    return withDb(async (database) => {
      const result = await database.runAsync('DELETE FROM attendance WHERE synced = 1');
      return result.changes;
    });
  },

  async getStats(): Promise<{ total: number; synced: number; unsynced: number }> {
    return withDb(async (database) => {
      const total = await database.getFirstAsync<{ 'COUNT(*)': number }>(
        'SELECT COUNT(*) FROM attendance',
      );
      const synced = await database.getFirstAsync<{ 'COUNT(*)': number }>(
        'SELECT COUNT(*) FROM attendance WHERE synced = 1',
      );
      const unsynced = await database.getFirstAsync<{ 'COUNT(*)': number }>(
        'SELECT COUNT(*) FROM attendance WHERE synced = 0',
      );
      return {
        total: total?.['COUNT(*)'] ?? 0,
        synced: synced?.['COUNT(*)'] ?? 0,
        unsynced: unsynced?.['COUNT(*)'] ?? 0,
      };
    });
  },

  async savePersons(persons: Person[]): Promise<void> {
    return withDb(async (database) => {
      const now = new Date().toISOString();
      for (const p of persons) {
        await database.runAsync(
          'INSERT OR REPLACE INTO persons (id, name, last_sync) VALUES (?, ?, ?)',
          p.id,
          p.name,
          now,
        );
      }
    });
  },

  async getPersons(): Promise<Person[]> {
    return withDb(async (database) => {
      return await database.getAllAsync<Person>('SELECT * FROM persons ORDER BY name ASC');
    });
  },

  async logSync(batchId: string, count: number, success: boolean): Promise<void> {
    return withDb(async (database) => {
      await database.runAsync(
        'INSERT INTO sync_log (batch_id, timestamp, record_count, success) VALUES (?, ?, ?, ?)',
        batchId,
        new Date().toISOString(),
        count,
        success ? 1 : 0,
      );
    });
  },

  async getSyncLogs(limit = 20): Promise<SyncLog[]> {
    return withDb(async (database) => {
      return await database.getAllAsync<SyncLog>(
        'SELECT * FROM sync_log ORDER BY timestamp DESC LIMIT ?',
        limit,
      );
    });
  },
};
