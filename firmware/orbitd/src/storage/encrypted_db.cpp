/**
 * @file encrypted_db.cpp
 * @brief SQLCipher database implementation.
 */
#include "storage/encrypted_db.h"
#include <sqlcipher/sqlite3.h>
#include <iostream>

namespace orbit::storage {

EncryptedDB::EncryptedDB(const core::Config& config) {
    db_path_ = config.get_string("storage.db_path", "/opt/orbitd/data/earbrain.db");
    encryption_key_ = config.get_string("storage.db_key", "");
}

EncryptedDB::~EncryptedDB() { close(); }

bool EncryptedDB::open() {
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "[db] Open failed: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // Apply encryption key
    if (!encryption_key_.empty()) {
        std::string pragma = "PRAGMA key = '" + encryption_key_ + "';";
        rc = sqlite3_exec(db_, pragma.c_str(), nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "[db] Encryption key failed" << std::endl;
            close();
            return false;
        }
    }

    // Optimize for embedded: WAL mode, reduced KDF iterations
    execute("PRAGMA journal_mode = WAL;");
    execute("PRAGMA synchronous = NORMAL;");
    execute("PRAGMA temp_store = MEMORY;");
    execute("PRAGMA cache_size = -2000;");  // 2MB cache

    // Create tables
    execute(R"(
        CREATE TABLE IF NOT EXISTS calls (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            start_time TEXT NOT NULL,
            end_time TEXT,
            duration_seconds INTEGER,
            caller_hash TEXT,
            summary_json TEXT,
            crm_json TEXT,
            email_draft TEXT,
            talk_time_json TEXT,
            created_at TEXT DEFAULT (datetime('now'))
        );
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS action_items (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            call_id INTEGER REFERENCES calls(id),
            who TEXT,
            what TEXT NOT NULL,
            due_date TEXT,
            priority TEXT DEFAULT 'medium',
            completed INTEGER DEFAULT 0,
            created_at TEXT DEFAULT (datetime('now'))
        );
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS decisions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            call_id INTEGER REFERENCES calls(id),
            decision_text TEXT NOT NULL,
            parties TEXT,
            binding INTEGER DEFAULT 0,
            created_at TEXT DEFAULT (datetime('now'))
        );
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS contacts (
            phone_hash TEXT PRIMARY KEY,
            display_name TEXT,
            context_json TEXT,
            last_call TEXT,
            call_count INTEGER DEFAULT 0
        );
    )");

    std::cout << "[db] Database opened: " << db_path_ << std::endl;
    return true;
}

void EncryptedDB::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool EncryptedDB::execute(const std::string& sql) {
    if (!db_) return false;
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "[db] SQL error: " << (err ? err : "unknown") << std::endl;
        sqlite3_free(err);
        return false;
    }
    return true;
}

std::string EncryptedDB::query_json(const std::string& sql) {
    // Simplified — in production use sqlite3_step with column inspection
    // to build JSON dynamically
    return "[]";
}

}  // namespace orbit::storage
