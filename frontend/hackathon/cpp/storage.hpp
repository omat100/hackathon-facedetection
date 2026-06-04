#pragma once
#include <string>
#include <vector>
#include <sqlite3.h>

struct AttendanceRecord {
    int id;
    std::string person_id;
    std::string timestamp;
    double confidence;
};

struct AttendanceStats {
    int total;
    int unsynced;
    int synced;
};

class AttendanceStorage {
public:
    AttendanceStorage(const std::string& db_path = "attendance.db");
    ~AttendanceStorage();

    void log(const std::string& person_id, double confidence);
    std::vector<AttendanceRecord> get_unsynced();
    void mark_synced(const std::vector<int>& ids);
    int purge_synced();
    AttendanceStats stats();

private:
    void init_db();
    sqlite3* db_;
    std::string db_path_;
};
