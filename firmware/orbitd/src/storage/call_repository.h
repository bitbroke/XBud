/**
 * @file call_repository.h / call_repository.cpp
 * @brief CRUD operations for call records in the encrypted database.
 */
#pragma once
#include "storage/encrypted_db.h"
#include <string>
#include <cstdint>

namespace orbit::storage {

class CallRepository {
public:
    explicit CallRepository(EncryptedDB& db) : db_(db) {}

    int64_t create_call(const std::string& caller_hash);
    void end_call(int64_t call_id, int duration_seconds);
    void save_summary(const std::string& summary_json);
    void save_action_items(int64_t call_id, const std::string& items_json);
    void save_decisions(int64_t call_id, const std::string& decisions_json);

private:
    EncryptedDB& db_;
    int64_t current_call_id_ = -1;
};

}  // namespace orbit::storage
