/**
 * @file relationship_store.h / .cpp
 * @brief Key-value contact memory store (v1 simplified from graph DB).
 */
#pragma once
#include "storage/encrypted_db.h"
#include <string>

namespace orbit::storage {

class RelationshipStore {
public:
    explicit RelationshipStore(EncryptedDB& db) : db_(db) {}

    void update_contact(const std::string& phone_hash,
                       const std::string& name,
                       const std::string& context_json);
    std::string get_contact_context(const std::string& phone_hash);
    void increment_call_count(const std::string& phone_hash);

private:
    EncryptedDB& db_;
};

}  // namespace orbit::storage
