/**
 * @file prompt_templates.cpp
 * @brief Task-specific prompt construction for Gemma 3 1B.
 *
 * Prompts are kept minimal to fit within the 2048-token context window.
 * Each prompt uses Gemma's chat template format.
 */

#include "slm/prompt_templates.h"

namespace orbit::slm {

PromptTemplates::PromptTemplates(const core::Config& config) {
    system_prefix_ = config.get_string("slm.system_prefix",
        "You are a concise business call analyzer. "
        "Extract structured information from call transcripts. "
        "Output ONLY valid JSON matching the required schema. "
        "Do not hallucinate information not present in the transcript.");
}

std::string PromptTemplates::wrap_prompt(
    const std::string& system,
    const std::string& task,
    const std::string& transcript)
{
    // Gemma chat template format
    return "<start_of_turn>user\n"
           + system + "\n\n"
           + task + "\n\n"
           "TRANSCRIPT:\n" + transcript + "\n"
           "<end_of_turn>\n"
           "<start_of_turn>model\n";
}

std::string PromptTemplates::build_action_items(const std::string& transcript) const {
    return wrap_prompt(system_prefix_,
        "Extract action items from the transcript. "
        "For each action item, identify: who (assignee), what (task), when (deadline as ISO-8601). "
        "Output a JSON array of action items. "
        "If no action items found, output an empty array [].",
        transcript);
}

std::string PromptTemplates::build_calendar_events(const std::string& transcript) const {
    return wrap_prompt(system_prefix_,
        "Extract any meetings, appointments, or scheduling agreements. "
        "For each, identify: title, participants, datetime (ISO-8601), duration_minutes. "
        "Handle relative dates like 'tomorrow', 'next Tuesday', 'parso' (day after tomorrow). "
        "Today's date context will be provided. Output JSON array.",
        transcript);
}

std::string PromptTemplates::build_decisions(const std::string& transcript) const {
    return wrap_prompt(system_prefix_,
        "Extract binding decisions and commitments from the transcript. "
        "Look for phrases like 'we agree', 'let's go with', 'confirmed', 'deal done'. "
        "For each, output: decision_text, parties_involved, context. "
        "Output JSON array.",
        transcript);
}

std::string PromptTemplates::build_summary(const std::string& transcript) const {
    return wrap_prompt(system_prefix_,
        "Generate a structured call summary with these sections: "
        "1. executive_overview (2-3 sentences), "
        "2. key_takeaways (bullet points), "
        "3. action_items (who/what/when), "
        "4. next_steps, "
        "5. sentiment (overall tone). "
        "Output as JSON object.",
        transcript);
}

std::string PromptTemplates::build_crm_payload(const std::string& transcript) const {
    return wrap_prompt(system_prefix_,
        "Extract CRM-relevant fields from this call: "
        "deal_stage, budget_mentioned, authority_confirmed, need_identified, "
        "timeline, competitor_mentions, objections_raised, next_action. "
        "Follow the BANT/MEDDIC framework. Output JSON object.",
        transcript);
}

std::string PromptTemplates::build_email_draft(const std::string& transcript) const {
    return wrap_prompt(system_prefix_,
        "Draft a professional follow-up email based on this call. "
        "Include: subject, greeting, summary of discussion, "
        "confirmed action items, next meeting details if any, "
        "and a polite closing. Output JSON with fields: "
        "subject, body, priority.",
        transcript);
}

std::string PromptTemplates::build_key_points(const std::string& transcript) const {
    return wrap_prompt(system_prefix_,
        "Identify the top 3-5 key points from this transcript segment. "
        "Focus on decisions, requirements, quantitative metrics, and deadlines. "
        "Rank by importance. Output JSON array of key_point objects.",
        transcript);
}

std::string PromptTemplates::build_silence_prompt(const std::string& transcript) const {
    return wrap_prompt(system_prefix_,
        "Based on the conversation so far, suggest: "
        "1. Any unaddressed topics that should be discussed, "
        "2. A natural conversation continuation prompt. "
        "Keep suggestions brief and actionable. Output JSON.",
        transcript);
}

}  // namespace orbit::slm
