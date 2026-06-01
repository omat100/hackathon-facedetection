import sqlite3
import os
from datetime import datetime

DB_PATH = "attendance.db"

class AttendanceStorage:
    def __init__(self, db_path=DB_PATH):
        self.db_path = db_path
        self._init_db()
        print("SQLite storage ready!")

    def _init_db(self):
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS attendance (
                    id          INTEGER PRIMARY KEY AUTOINCREMENT,
                    person_id   TEXT NOT NULL,
                    timestamp   TEXT NOT NULL,
                    confidence  REAL NOT NULL,
                    synced      INTEGER DEFAULT 0
                )
            """)
            conn.commit()

    def log(self, person_id, confidence):
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("""
                INSERT INTO attendance (person_id, timestamp, confidence, synced)
                VALUES (?, ?, ?, 0)
            """, (person_id, datetime.now().isoformat(), round(confidence, 4)))
            conn.commit()
        print(f"Logged: {person_id} at {datetime.now().isoformat()}")

    def get_unsynced(self):
        with sqlite3.connect(self.db_path) as conn:
            rows = conn.execute("""
                SELECT id, person_id, timestamp, confidence
                FROM attendance
                WHERE synced = 0
            """).fetchall()
        return [
            {"id": r[0], "person_id": r[1], "timestamp": r[2], "confidence": r[3]}
            for r in rows
        ]

    def mark_synced(self, ids):
        with sqlite3.connect(self.db_path) as conn:
            conn.executemany(
                "UPDATE attendance SET synced = 1 WHERE id = ?",
                [(i,) for i in ids]
            )
            conn.commit()

    def purge_synced(self):
        with sqlite3.connect(self.db_path) as conn:
            deleted = conn.execute(
                "DELETE FROM attendance WHERE synced = 1"
            ).rowcount
            conn.commit()
        print(f"🗑️ Purged {deleted} synced records")
        return deleted

    def stats(self):
        with sqlite3.connect(self.db_path) as conn:
            total    = conn.execute("SELECT COUNT(*) FROM attendance").fetchone()[0]
            unsynced = conn.execute("SELECT COUNT(*) FROM attendance WHERE synced = 0").fetchone()[0]
            synced   = conn.execute("SELECT COUNT(*) FROM attendance WHERE synced = 1").fetchone()[0]
        return {"total": total, "unsynced": unsynced, "synced": synced}


if __name__ == "__main__":
    db = AttendanceStorage()

    # Test
    db.log("Om Thakur", 0.87)

    print(f"\nStats: {db.stats()}")
    print(f"Unsynced: {db.get_unsynced()}")