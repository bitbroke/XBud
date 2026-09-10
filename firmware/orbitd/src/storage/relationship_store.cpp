/**
 * @file relationship_store.cpp
 */
#include "storage/relationship_store.h"

namespace orbit::storage {

void RelationshipStore::update_contact(const std::string& phone_hash,
                                        const std::string& name,
                                        const std::string& context_json) {
    std::string sql = "INSERT OR REPLACE INTO contacts (phone_hash, display_name, context_json, last_call) "
                      "VALUES ('" + phone_hash + "', '" + name + "', '" +
                      context_json + "', datetime('now'));";
    db_.execute(sql);
}

std::string RelationshipStore::get_contact_context(const std::string& phone_hash) {
    return db_.query_json("SELECT context_json FROM contacts WHERE phone_hash = '" +
                          phone_hash + "';");
}

void RelationshipStore::increment_call_count(const std::string& phone_hash) {
    db_.execute("UPDATE contacts SET call_count = call_count + 1, last_call = datetime('now') "
                "WHERE phone_hash = '" + phone_hash + "';");
}

}  // namespace orbit::storage
