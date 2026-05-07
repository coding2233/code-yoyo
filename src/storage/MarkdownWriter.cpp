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
