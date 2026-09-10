/**
 * @file encrypted_db.h
 * @brief SQLCipher-encrypted SQLite database wrapper.
 */
#pragma once
#include "core/config.h"
#include <string>
struct sqlite3;

namespace orbit::storage {

class EncryptedDB {
public:
    explicit EncryptedDB(const core::Config& config);
    ~EncryptedDB();

    bool open();
    void close();
    bool is_open() const { return db_ != nullptr; }

    /** Execute a SQL statement (no result). */
    bool execute(const std::string& sql);

    /** Execute a SQL query and return results as a JSON string. */
    std::string query_json(const std::string& sql);

    /** Get the underlying SQLite handle (for repositories). */
    sqlite3* handle() { return db_; }

private:
    std::string db_path_;
    std::string encryption_key_;
    sqlite3* db_ = nullptr;
};

}  // namespace orbit::storage
