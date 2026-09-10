/**
 * @file decision_ledger.cpp
 */
#include "storage/decision_ledger.h"

namespace orbit::storage {

void DecisionLedger::record(int64_t call_id, const std::string& text,
                             const std::string& parties, bool binding) {
    std::string sql = "INSERT INTO decisions (call_id, decision_text, parties, binding) VALUES (" +
                      std::to_string(call_id) + ", '" + text + "', '" +
                      parties + "', " + (binding ? "1" : "0") + ");";
    db_.execute(sql);
}

std::string DecisionLedger::search(const std::string& keyword) {
    return db_.query_json(
        "SELECT * FROM decisions WHERE decision_text LIKE '%" + keyword + "%' "
        "ORDER BY created_at DESC LIMIT 20;");
}

}  // namespace orbit::storage
