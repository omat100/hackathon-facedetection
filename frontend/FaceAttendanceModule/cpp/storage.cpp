#include "storage.hpp"
#include <iostream>
#include <sstream>
#include <ctime>

AttendanceStorage::AttendanceStorage(const std::string& db_path)
    : db_(nullptr), db_path_(db_path) {
    init_db();
    std::cout << "SQLite storage ready!" << std::endl;
}

AttendanceStorage::~AttendanceStorage() {
    if (db_) sqlite3_close(db_);
}

void AttendanceStorage::init_db() {
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open database: " << sqlite3_errmsg(db_) << std::endl;
        return;
    }
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS attendance (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            person_id   TEXT NOT NULL,
            timestamp   TEXT NOT NULL,
            confidence  REAL NOT NULL,
            synced      INTEGER DEFAULT 0
        )
    )";
    char* err = nullptr;
    rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << err << std::endl;
        sqlite3_free(err);
    }
}

void AttendanceStorage::log(const std::string& person_id, double confidence) {
    auto now = std::time(nullptr);
    char ts[64];
    std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));

    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO attendance (person_id, timestamp, confidence, synced) VALUES (?, ?, ?, 0)";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, person_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, ts, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, confidence);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    std::cout << "Logged: " << person_id << " at " << ts << std::endl;
}

std::vector<AttendanceRecord> AttendanceStorage::get_unsynced() {
    std::vector<AttendanceRecord> records;
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, person_id, timestamp, confidence FROM attendance WHERE synced = 0";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AttendanceRecord r;
        r.id = sqlite3_column_int(stmt, 0);
        r.person_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.confidence = sqlite3_column_double(stmt, 3);
        records.push_back(r);
    }
    sqlite3_finalize(stmt);
    return records;
}

void AttendanceStorage::mark_synced(const std::vector<int>& ids) {
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE attendance SET synced = 1 WHERE id = ?";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    for (int id : ids) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
}

int AttendanceStorage::purge_synced() {
    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM attendance WHERE synced = 1";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_step(stmt);
    int deleted = sqlite3_changes(db_);
    sqlite3_finalize(stmt);
    std::cout << "Purged " << deleted << " synced records" << std::endl;
    return deleted;
}

AttendanceStats AttendanceStorage::stats() {
    AttendanceStats s{};
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM attendance", -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) s.total = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM attendance WHERE synced = 0", -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) s.unsynced = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM attendance WHERE synced = 1", -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) s.synced = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return s;
}
