/**
 * @file prompt_templates.h
 * @brief Task-specific prompt builders for the Gemma SLM.
 *
 * Each method constructs a prompt optimized for the target GBNF grammar,
 * including system instructions, task context, and the transcript snippet.
 */
#pragma once

#include "core/config.h"
#include <string>

namespace orbit::slm {

class PromptTemplates {
public:
    explicit PromptTemplates(const core::Config& config);
    ~PromptTemplates() = default;

    /** Build prompt for W3 action item extraction (Who/What/When). */
    std::string build_action_items(const std::string& transcript) const;

    /** Build prompt for calendar event extraction. */
    std::string build_calendar_events(const std::string& transcript) const;

    /** Build prompt for decision/commitment detection. */
    std::string build_decisions(const std::string& transcript) const;

    /** Build prompt for full call summary (post-call). */
    std::string build_summary(const std::string& transcript) const;

    /** Build prompt for CRM field auto-population. */
    std::string build_crm_payload(const std::string& transcript) const;

    /** Build prompt for follow-up email draft. */
    std::string build_email_draft(const std::string& transcript) const;

    /** Build prompt for key point highlighting. */
    std::string build_key_points(const std::string& transcript) const;

    /** Build prompt for silence utilization (conversation continuation). */
    std::string build_silence_prompt(const std::string& transcript) const;

private:
    std::string system_prefix_;

    static std::string wrap_prompt(const std::string& system,
                                    const std::string& task,
                                    const std::string& transcript);
};

}  // namespace orbit::slm
