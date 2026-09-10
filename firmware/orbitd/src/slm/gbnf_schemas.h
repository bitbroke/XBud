/**
 * @file gbnf_schemas.h
 * @brief GBNF grammar file paths and schema references.
 *
 * This header defines the mapping between task names and their
 * corresponding GBNF grammar files used to constrain SLM output.
 */
#pragma once

namespace orbit::slm {

// Grammar file names (without .gbnf extension)
namespace grammar {
    constexpr const char* ACTION_ITEM    = "action_item";
    constexpr const char* CALL_SUMMARY   = "call_summary";
    constexpr const char* CALENDAR_EVENT = "calendar_event";
    constexpr const char* CRM_PAYLOAD    = "crm_payload";
    constexpr const char* EMAIL_DRAFT    = "email_draft";
    constexpr const char* DECISION_ENTRY = "decision_entry";
}  // namespace grammar

}  // namespace orbit::slm
