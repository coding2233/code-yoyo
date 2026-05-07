#include "MarkdownWriter.h"
#include <sstream>
#include <algorithm>

std::string MarkdownWriter::UpdateSubtaskStatus(
    const std::string& content,
    const std::string& subtask_id,
    const std::string& new_status,
    const std::string& result)
{
    std::istringstream in(content);
    std::ostringstream out;
    std::string line;
    bool in_target_sub = false;
    bool status_updated = false;
    bool result_updated = false;

    while (std::getline(in, line)) {
        // Detect entering the target subtask
        if (line.find("### " + subtask_id + ":") == 0) {
            in_target_sub = true;
        } else if (in_target_sub && line.find("### ") == 0) {
            // Next subtask started
            in_target_sub = false;
        }

        // Update status line
        if (in_target_sub && !status_updated &&
            (line.find("- **status**:") != std::string::npos ||
             line.find("**status**:") != std::string::npos))
        {
            auto prefix = line.find("**status**:");
            if (prefix != std::string::npos) {
                auto space = line.rfind(' ', prefix - 2);
                auto before = (space != std::string::npos && line[space] == '-')
                    ? line.substr(0, space)
                    : line.substr(0, prefix);
                // Preserve indentation
                out << before << "- **status**: " << new_status << "\n";
                status_updated = true;
                continue;
            }
        }

        // Update or add result line
        if (in_target_sub && !result.empty()) {
            if (line.find("- **result**:") != std::string::npos) {
                auto prefix = line.find("- **result**:");
                auto before = line.substr(0, prefix);
                out << before << "- **result**: " << result << "\n";
                result_updated = true;
                continue;
            }
        }

        out << line << "\n";
    }

    // If result line didn't exist, we'd need to add it - but for Phase 1 we skip this complexity
    return out.str();
}

std::string MarkdownWriter::AddConversationMsg(
    const std::string& content,
    const std::string& subtask_id,
    const std::string& sender,
    const std::string& timestamp,
    const std::string& body,
    bool is_exec_log)
{
    std::istringstream in(content);
    std::ostringstream out;
    std::string line;
    bool in_target_sub = false;
    bool in_separator = false;
    bool added = false;

    // Find the "---" after the target subtask and insert before it
    std::vector<std::string> lines;
    std::string current_line;
    while (std::getline(in, current_line)) {
        lines.push_back(current_line);
    }

    int target_end = -1;
    for (int i = 0; i < (int)lines.size(); i++) {
        if (lines[i].find("### " + subtask_id + ":") == 0) {
            target_end = i;
        }
        if (target_end > 0 && lines[i] == "---" && i > target_end) {
            // Found the separator after the subtask
            target_end = i;
            break;
        }
    }

    for (int i = 0; i < (int)lines.size(); i++) {
        out << lines[i] << "\n";

        // Insert message right before the "---" separator after the subtask
        if (target_end > 0 && i == target_end - 1 && !added) {
            if (is_exec_log) {
                out << "> #### \u6267\u884c\u65e5\u5fd7\n>\n> ```\n";
                std::istringstream body_stream(body);
                std::string bl;
                while (std::getline(body_stream, bl)) {
                    out << "> " << bl << "\n";
                }
                out << "> ```\n>\n";
            } else {
                out << "> **[" << sender << "]** (" << timestamp << ")\n";
                std::istringstream body_stream(body);
                std::string bl;
                while (std::getline(body_stream, bl)) {
                    out << "> " << bl << "\n";
                }
                out << ">\n";
            }
            added = true;
        }
    }

    return out.str();
}

std::string MarkdownWriter::UpdateSchedule(
    const std::string& content,
    const std::string& last_run,
    const std::string& next_run)
{
    std::istringstream in(content);
    std::ostringstream out;
    std::string line;
    bool last_updated = false;
    bool next_updated = false;

    while (std::getline(in, line)) {
        if (!last_updated && line.find("**last_run**:") != std::string::npos) {
            auto prefix = line.find("**last_run**:");
            if (prefix != std::string::npos) {
                auto before = line.substr(0, prefix);
                out << before << "**last_run**: " << last_run << "\n";
                last_updated = true;
                continue;
            }
        }
        if (!next_updated && line.find("**next_run**:") != std::string::npos) {
            auto prefix = line.find("**next_run**:");
            if (prefix != std::string::npos) {
                auto before = line.substr(0, prefix);
                out << before << "**next_run**: " << next_run << "\n";
                next_updated = true;
                continue;
            }
        }
        out << line << "\n";
    }

    return out.str();
}

std::string MarkdownWriter::UpdateTaskSubtasks(
    const std::string& content,
    const std::vector<Subtask>& subtasks)
{
    std::istringstream in(content);
    std::ostringstream out;
    std::string line;
    bool in_subtasks = false;
    bool subtasks_replaced = false;

    while (std::getline(in, line)) {
        if (line.find("## Subtasks") == 0) {
            in_subtasks = true;
            subtasks_replaced = true;
            out << "## Subtasks\n\n";
            for (const auto& sub : subtasks) {
                auto risk_str = [](RiskLevel r) {
                    switch (r) { case RiskLevel::Low: return "low"; case RiskLevel::Medium: return "medium"; case RiskLevel::High: return "high"; }
                    return "medium";
                };
                auto status_str = [](SubtaskStatus s) {
                    switch (s) { case SubtaskStatus::Pending: return "pending"; case SubtaskStatus::InProgress: return "in_progress"; case SubtaskStatus::Review: return "review"; case SubtaskStatus::Completed: return "completed"; case SubtaskStatus::Failed: return "failed"; case SubtaskStatus::Cancelled: return "cancelled"; }
                    return "pending";
                };
                auto approval_str = [](ApprovalMode a) {
                    switch (a) { case ApprovalMode::Auto: return "auto"; case ApprovalMode::Confirm: return "confirm"; case ApprovalMode::AgentDecides: return "agent_decides"; }
                    return "auto";
                };
                auto exec_str = [](ExecMode e) {
                    switch (e) { case ExecMode::Parallel: return "parallel"; case ExecMode::Serial: return "serial"; }
                    return "serial";
                };

                out << "### " << sub.id << ": " << sub.title << "\n\n";
                out << "- **status**: " << status_str(sub.status) << "\n";
                if (!sub.assignee.empty()) out << "- **assignee**: " << sub.assignee << "\n";
                out << "- **risk**: " << risk_str(sub.risk) << "\n";
                out << "- **approval**: " << approval_str(sub.approval) << "\n";
                out << "- **execution**: " << exec_str(sub.exec_mode) << "\n";
                if (!sub.depends.empty()) {
                    out << "- **depends**: ";
                    for (size_t d = 0; d < sub.depends.size(); d++) {
                        if (d > 0) out << ", ";
                        out << sub.depends[d];
                    }
                    out << "\n";
                }
                if (!sub.result.empty()) out << "- **result**: " << sub.result << "\n";
                if (!sub.description.empty()) {
                    out << "\n" << sub.description << "\n";
                }
                out << "\n---\n\n";
            }
            // Skip old subtask lines
            while (std::getline(in, line)) {
                if (line.find("## ") == 0) {
                    // Reached next section, write it and continue
                    out << line << "\n";
                    in_subtasks = false;
                    break;
                }
            }
            continue;
        }
        if (!in_subtasks) {
            out << line << "\n";
        }
    }

    if (!subtasks_replaced) {
        out << "## Subtasks\n\n";
        for (const auto& sub : subtasks) {
            auto risk_str = [](RiskLevel r) {
                switch (r) { case RiskLevel::Low: return "low"; case RiskLevel::Medium: return "medium"; case RiskLevel::High: return "high"; }
                return "medium";
            };
            auto status_str = [](SubtaskStatus s) {
                switch (s) { case SubtaskStatus::Pending: return "pending"; case SubtaskStatus::InProgress: return "in_progress"; case SubtaskStatus::Review: return "review"; case SubtaskStatus::Completed: return "completed"; case SubtaskStatus::Failed: return "failed"; case SubtaskStatus::Cancelled: return "cancelled"; }
                return "pending";
            };
            auto approval_str = [](ApprovalMode a) {
                switch (a) { case ApprovalMode::Auto: return "auto"; case ApprovalMode::Confirm: return "confirm"; case ApprovalMode::AgentDecides: return "agent_decides"; }
                return "auto";
            };
            auto exec_str = [](ExecMode e) {
                switch (e) { case ExecMode::Parallel: return "parallel"; case ExecMode::Serial: return "serial"; }
                return "serial";
            };
            out << "### " << sub.id << ": " << sub.title << "\n\n";
            out << "- **status**: " << status_str(sub.status) << "\n";
            if (!sub.assignee.empty()) out << "- **assignee**: " << sub.assignee << "\n";
            out << "- **risk**: " << risk_str(sub.risk) << "\n";
            out << "- **approval**: " << approval_str(sub.approval) << "\n";
            out << "- **execution**: " << exec_str(sub.exec_mode) << "\n";
            if (!sub.description.empty()) {
                out << "\n" << sub.description << "\n";
            }
            out << "\n---\n\n";
        }
    }

    return out.str();
}

std::string MarkdownWriter::FormatAuditEntry(
    const std::string& timestamp,
    const std::string& action,
    const std::string& actor,
    const std::string& detail)
{
    std::ostringstream out;
    out << "## " << timestamp << "\n";
    out << "- **action**: " << action << "\n";
    out << "- **actor**: " << actor << "\n";
    out << "- **detail**: " << detail << "\n\n";
    return out.str();
}
