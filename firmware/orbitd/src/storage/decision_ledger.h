/**
 * @file decision_ledger.h / .cpp
 * @brief Immutable decision log stored in SQLCipher.
 */
#pragma once
#include "storage/encrypted_db.h"
#include <string>

namespace orbit::storage {

class DecisionLedger {
public:
    explicit DecisionLedger(EncryptedDB& db) : db_(db) {}
    void record(int64_t call_id, const std::string& text,
                const std::string& parties, bool binding);
    std::string search(const std::string& keyword);

private:
    EncryptedDB& db_;
};

}  // namespace orbit::storage
