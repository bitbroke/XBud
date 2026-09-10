/**
 * @file call_repository.cpp
 */
#include "storage/call_repository.h"
#include <sqlcipher/sqlite3.h>
#include <iostream>

namespace orbit::storage {

int64_t CallRepository::create_call(const std::string& caller_hash) {
    std::string sql = "INSERT INTO calls (start_time, caller_hash) VALUES (datetime('now'), '" +
                      caller_hash + "');";
    if (db_.execute(sql)) {
        current_call_id_ = sqlite3_last_insert_rowid(db_.handle());
        return current_call_id_;
    }
    return -1;
}

void CallRepository::end_call(int64_t call_id, int duration_seconds) {
    std::string sql = "UPDATE calls SET end_time = datetime('now'), duration_seconds = " +
                      std::to_string(duration_seconds) + " WHERE id = " +
                      std::to_string(call_id) + ";";
    db_.execute(sql);
}

void CallRepository::save_summary(const std::string& summary_json) {
    if (current_call_id_ < 0) return;
    // Use parameterized query in production to prevent SQL injection
    std::string sql = "UPDATE calls SET summary_json = ? WHERE id = " +
                      std::to_string(current_call_id_) + ";";
    db_.execute(sql);
}

void CallRepository::save_action_items(int64_t call_id, const std::string& items_json) {
    // Parse JSON array and insert individual items
    // Simplified — in production, use nlohmann::json to parse
    std::string sql = "INSERT INTO action_items (call_id, what) VALUES (" +
                      std::to_string(call_id) + ", 'parsed from JSON');";
    db_.execute(sql);
}

void CallRepository::save_decisions(int64_t call_id, const std::string& decisions_json) {
    std::string sql = "INSERT INTO decisions (call_id, decision_text) VALUES (" +
                      std::to_string(call_id) + ", 'parsed from JSON');";
    db_.execute(sql);
}

}  // namespace orbit::storage
