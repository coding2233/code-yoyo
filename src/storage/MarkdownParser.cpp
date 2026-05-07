#include "MarkdownParser.h"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <filesystem>
namespace fs = std::filesystem;

std::string MarkdownParser::Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool MarkdownParser::StartsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

bool MarkdownParser::EndsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

std::string MarkdownParser::ExtractBoldValue(const std::string& line, const std::string& key) {
    std::string pattern = "**" + key + "**: ";
    auto pos = line.find(pattern);
    if (pos == std::string::npos) {
        pattern = "- **" + key + "**: ";
        pos = line.find(pattern);
    }
    if (pos == std::string::npos) return "";
    auto val_start = pos + pattern.size();
    auto val_end = line.find_first_of(" \t\r\n", val_start);
    if (val_end == std::string::npos) return Trim(line.substr(val_start));
    return Trim(line.substr(val_start, val_end - val_start));
}

// ---- Projects Index ----

std::vector<Project> MarkdownParser::ParseProjectsIndex(const std::string& content) {
    std::vector<Project> projects;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        auto trimmed = Trim(line);
        // Format: - [name] path — status
        if (StartsWith(trimmed, "- [")) {
            Project p;
            auto close_bracket = trimmed.find(']');
            if (close_bracket != std::string::npos) {
                p.name = Trim(trimmed.substr(3, close_bracket - 3));
            }
            auto space_after = trimmed.find(' ', close_bracket + 1);
            if (space_after != std::string::npos) {
                auto emdash = trimmed.find("\u2014", space_after + 1);
                if (emdash != std::string::npos) {
                    p.repo = Trim(trimmed.substr(space_after + 1, emdash - space_after - 1));
                    p.status = Trim(trimmed.substr(emdash + 3));
                } else {
                    p.repo = Trim(trimmed.substr(space_after + 1));
                }
            }
            projects.push_back(p);
        }
    }
    return projects;
}

std::string MarkdownParser::FormatProjectsIndex(const std::vector<Project>& projects) {
    std::ostringstream out;
    out << "# Projects\n\n";
    for (const auto& p : projects) {
        out << "- [" << p.name << "] " << p.repo << " \u2014 " << p.status << "\n";
    }
    return out.str();
}

// ---- Project ----

Project MarkdownParser::ParseProject(const std::string& content, const std::string& codeyoyo_path) {
    Project p;
    p.codeyoyo_path = codeyoyo_path;
    p.tasks_path = codeyoyo_path + "/tasks";
    p.sessions_path = codeyoyo_path + "/sessions";

    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        auto trimmed = Trim(line);
        if (StartsWith(trimmed, "# Project:")) {
            p.name = Trim(trimmed.substr(10));
        } else if (auto v = ExtractBoldValue(trimmed, "repo"); !v.empty()) {
            p.repo = v;
        } else if (auto v = ExtractBoldValue(trimmed, "created"); !v.empty()) {
            p.created = v;
        } else if (auto v = ExtractBoldValue(trimmed, "status"); !v.empty()) {
            p.status = v;
        }
    }
    return p;
}

std::string MarkdownParser::FormatProject(const Project& p) {
    std::ostringstream out;
    out << "# Project: " << p.name << "\n\n";
    out << "**repo**: " << p.repo << "\n";
    out << "**created**: " << p.created << "\n";
    out << "**status**: " << p.status << "\n";
    out << "\n## Tasks\n\n";
    // Task list will be filled in by ProjectManager
    return out.str();
}

// ---- Task ----

Task MarkdownParser::ParseTask(const std::string& content) {
    Task task;
    std::istringstream stream(content);
    std::string line;
    Subtask* current_sub = nullptr;
    ConversationMsg* current_msg = nullptr;
    bool in_subtasks = false;
    bool in_exec_log = false;
    bool in_code_block = false;
    std::string code_block_acc;

    while (std::getline(stream, line)) {
        auto trimmed = Trim(line);

        // Heading 1: Task title
        if (StartsWith(trimmed, "# ") && !StartsWith(trimmed, "## ")) {
            auto title_part = trimmed.substr(2);
            auto colon = title_part.find(':');
            if (colon != std::string::npos) {
                task.id = Trim(title_part.substr(0, colon));
                task.title = Trim(title_part.substr(colon + 1));
            } else {
                task.title = title_part;
            }
            continue;
        }

        // Heading 2: Subtasks section
        if (trimmed == "## Subtasks") {
            in_subtasks = true;
            continue;
        }

        // Heading 3: Subtask
        if (StartsWith(trimmed, "### ")) {
            in_subtasks = true;
            task.subtasks.push_back(Subtask());
            current_sub = &task.subtasks.back();
            current_msg = nullptr;
            in_exec_log = false;

            auto title_part = trimmed.substr(4);
            auto colon = title_part.find(':');
            if (colon != std::string::npos) {
                current_sub->id = Trim(title_part.substr(0, colon));
                current_sub->title = Trim(title_part.substr(colon + 1));
            } else {
                current_sub->title = title_part;
            }
            continue;
        }

        // Heading 4: Exec log
        if (trimmed == "#### \u6267\u884c\u65e5\u5fd7" && current_sub) {
            in_exec_log = true;
            current_msg = nullptr;
            continue;
        }

        if (trimmed == "---") {
            current_sub = nullptr;
            current_msg = nullptr;
            in_exec_log = false;
            continue;
        }

        // Metadata lines
        if ((StartsWith(trimmed, "**") || StartsWith(trimmed, "- **")) && !in_code_block) {
            auto v = ExtractBoldValue(trimmed, "status");
            if (!v.empty() && current_sub) {
                current_sub->status = task.ParseStatus(v);
                continue;
            }
            if (!v.empty() && !current_sub) {
                task.status = v;
                continue;
            }
            v = ExtractBoldValue(trimmed, "assignee");
            if (!v.empty() && current_sub) { current_sub->assignee = v; continue; }
            v = ExtractBoldValue(trimmed, "risk");
            if (!v.empty() && current_sub) {
                if (v == "low") current_sub->risk = RiskLevel::Low;
                else if (v == "medium") current_sub->risk = RiskLevel::Medium;
                else if (v == "high") current_sub->risk = RiskLevel::High;
                continue;
            }
            v = ExtractBoldValue(trimmed, "approval");
            if (!v.empty() && current_sub) {
                if (v == "auto") current_sub->approval = ApprovalMode::Auto;
                else if (v == "confirm") current_sub->approval = ApprovalMode::Confirm;
                else if (v == "agent_decides") current_sub->approval = ApprovalMode::AgentDecides;
                continue;
            }
            v = ExtractBoldValue(trimmed, "execution");
            if (!v.empty() && current_sub) {
                if (v == "parallel") current_sub->exec_mode = ExecMode::Parallel;
                else current_sub->exec_mode = ExecMode::Serial;
                continue;
            }
            v = ExtractBoldValue(trimmed, "result");
            if (!v.empty() && current_sub) { current_sub->result = v; continue; }
            v = ExtractBoldValue(trimmed, "priority");
            if (!v.empty() && !current_sub) { task.priority = v; continue; }
            v = ExtractBoldValue(trimmed, "created");
            if (!v.empty() && !current_sub) { task.created = v; continue; }
            v = ExtractBoldValue(trimmed, "updated");
            if (!v.empty() && !current_sub) { task.updated = v; continue; }
            v = ExtractBoldValue(trimmed, "schedule");
            if (!v.empty()) { task.schedule = v; continue; }
            v = ExtractBoldValue(trimmed, "timezone");
            if (!v.empty()) { task.timezone = v; continue; }
            v = ExtractBoldValue(trimmed, "next_run");
            if (!v.empty()) { task.next_run = v; continue; }
            v = ExtractBoldValue(trimmed, "last_run");
            if (!v.empty()) { task.last_run = v; continue; }
            v = ExtractBoldValue(trimmed, "depends");
            if (!v.empty() && current_sub && !in_subtasks) {
                // Parse depends list
                std::istringstream ds(v);
                std::string dep;
                while (std::getline(ds, dep, ',')) {
                    current_sub->depends.push_back(Trim(dep));
                }
                continue;
            }
            continue;
        }

        // Conversation messages
        if (StartsWith(trimmed, "> **") && EndsWith(trimmed, ")") && current_sub) {
            current_msg = nullptr;
            in_exec_log = false;
            auto sender_start = trimmed.find("**") + 2;
            auto sender_end = trimmed.find("**", sender_start);
            if (sender_start != std::string::npos && sender_end != std::string::npos) {
                ConversationMsg msg;
                msg.sender = Trim(trimmed.substr(sender_start, sender_end - sender_start));
                auto paren = trimmed.find('(');
                auto paren_end = trimmed.find(')');
                if (paren != std::string::npos && paren_end != std::string::npos) {
                    msg.timestamp = Trim(trimmed.substr(paren + 1, paren_end - paren - 1));
                    msg.body = "";
                }
                current_sub->conversation.push_back(msg);
                current_msg = &current_sub->conversation.back();
            }
            continue;
        }

        if (StartsWith(trimmed, "> ") && current_msg && !in_exec_log) {
            auto text = trimmed.substr(2);
            if (!current_msg->body.empty()) current_msg->body += "\n";
            current_msg->body += text;
            continue;
        }

        // Code block
        if (StartsWith(trimmed, "```")) {
            if (!in_code_block) {
                in_code_block = true;
                code_block_acc.clear();
            } else {
                in_code_block = false;
                if (in_exec_log && current_sub) {
                    ConversationMsg log_msg;
                    log_msg.sender = "";
                    log_msg.timestamp = "";
                    log_msg.body = code_block_acc;
                    log_msg.is_exec_log = true;
                    current_sub->conversation.push_back(log_msg);
                }
            }
            continue;
        }

        if (in_code_block && in_exec_log) {
            if (!code_block_acc.empty()) code_block_acc += "\n";
            code_block_acc += trimmed;
            continue;
        }

        // Description text (before subtasks section)
        if (!in_subtasks && !StartsWith(trimmed, "#") && !StartsWith(trimmed, "**") && !trimmed.empty()) {
            if (!task.description.empty()) task.description += "\n";
            task.description += trimmed;
        }
    }

    return task;
}

std::string MarkdownParser::FormatTask(const Task& task) {
    std::ostringstream out;
    out << "# " << task.id << ": " << task.title << "\n\n";
    out << "**status**: " << task.status << "\n";
    out << "**priority**: " << task.priority << "\n";
    out << "**created**: " << task.created << "\n";
    if (!task.updated.empty()) out << "**updated**: " << task.updated << "\n";
    if (!task.schedule.empty()) {
        out << "\n**schedule**: " << task.schedule << "\n";
        if (!task.timezone.empty()) out << "**timezone**: " << task.timezone << "\n";
        if (!task.next_run.empty()) out << "**next_run**: " << task.next_run << "\n";
        if (!task.last_run.empty()) out << "**last_run**: " << task.last_run << "\n";
    }

    if (!task.description.empty()) {
        out << "\n" << task.description << "\n";
    }

    out << "\n---\n\n## Subtasks\n\n";

    for (const auto& sub : task.subtasks) {
        out << "### " << sub.id << ": " << sub.title << "\n\n";
        out << "- **status**: " << task.StatusString(sub.status) << "\n";
        if (!sub.assignee.empty()) out << "- **assignee**: " << sub.assignee << "\n";
        out << "- **risk**: ";
        switch (sub.risk) {
            case RiskLevel::Low: out << "low"; break;
            case RiskLevel::Medium: out << "medium"; break;
            case RiskLevel::High: out << "high"; break;
        }
        out << "\n- **approval**: ";
        switch (sub.approval) {
            case ApprovalMode::Auto: out << "auto"; break;
            case ApprovalMode::Confirm: out << "confirm"; break;
            case ApprovalMode::AgentDecides: out << "agent_decides"; break;
        }
        out << "\n- **execution**: ";
        out << (sub.exec_mode == ExecMode::Parallel ? "parallel" : "serial");
        out << "\n";
        if (!sub.depends.empty()) {
            out << "- **depends**: ";
            for (size_t i = 0; i < sub.depends.size(); i++) {
                if (i > 0) out << ", ";
                out << sub.depends[i];
            }
            out << "\n";
        }
        if (!sub.result.empty()) {
            out << "- **result**: " << sub.result << "\n";
        }

        if (!sub.description.empty()) {
            out << "\n" << sub.description << "\n";
        }

        out << "\n";

        for (const auto& msg : sub.conversation) {
            if (msg.is_exec_log) {
                out << "> #### \u6267\u884c\u65e5\u5fd7\n>\n> ```\n";
                out << msg.body << "\n";
                out << "> ```\n>\n";
            } else {
                out << "> **[" << msg.sender << "]** (" << msg.timestamp << ")\n";
                std::istringstream msg_stream(msg.body);
                std::string msg_line;
                while (std::getline(msg_stream, msg_line)) {
                    out << "> " << msg_line << "\n";
                }
                out << ">\n";
            }
        }

        out << "---\n\n";
    }

    return out.str();
}

// ---- Agents ----

std::vector<Agent> MarkdownParser::ParseAgents(const std::string& content) {
    std::vector<Agent> agents;
    std::istringstream stream(content);
    std::string line;
    Agent* current = nullptr;
    bool in_instructions = false;
    bool in_env = false;
    bool in_skills = false;

    while (std::getline(stream, line)) {
        auto trimmed = Trim(line);

        if (StartsWith(trimmed, "## ")) {
            agents.push_back(Agent());
            current = &agents.back();
            current->id = Trim(trimmed.substr(3));
            in_instructions = false;
            in_env = false;
            in_skills = false;
            continue;
        }

        if (!current) continue;

        if (trimmed == "### instructions") {
            in_instructions = true;
            in_env = false;
            in_skills = false;
            continue;
        }
        if (trimmed == "### env") {
            in_instructions = false;
            in_env = true;
            in_skills = false;
            continue;
        }
        if (trimmed == "### skills") {
            in_instructions = false;
            in_env = false;
            in_skills = true;
            continue;
        }

        if (trimmed == "---") {
            current = nullptr;
            in_instructions = false;
            in_env = false;
            in_skills = false;
            continue;
        }

        if (in_instructions && !StartsWith(trimmed, "#") && !StartsWith(trimmed, "**")) {
            if (!current->instructions.empty()) current->instructions += "\n";
            current->instructions += trimmed;
            continue;
        }

        if (in_env) {
            auto eq = trimmed.find('=');
            if (eq != std::string::npos) {
                auto key = Trim(trimmed.substr(0, eq));
                auto val = Trim(trimmed.substr(eq + 1));
                if (!key.empty()) current->env[key] = val;
            }
            continue;
        }

        if (in_skills && StartsWith(trimmed, "- ")) {
            auto skill = Trim(trimmed.substr(2));
            if (!skill.empty()) current->skills.push_back(skill);
            continue;
        }

        // Metadata
        auto v = ExtractBoldValue(trimmed, "name");
        if (!v.empty()) { current->name = v; continue; }
        v = ExtractBoldValue(trimmed, "description");
        if (!v.empty()) { current->description = v; continue; }
        v = ExtractBoldValue(trimmed, "command");
        if (!v.empty()) { current->command = v; continue; }
        v = ExtractBoldValue(trimmed, "args");
        if (!v.empty()) { current->args = v; continue; }
        v = ExtractBoldValue(trimmed, "model");
        if (!v.empty()) { current->model = v; continue; }
        v = ExtractBoldValue(trimmed, "timeout");
        if (!v.empty()) { current->timeout = std::stoi(v); continue; }
        v = ExtractBoldValue(trimmed, "auto_approve");
        if (!v.empty()) { current->auto_approve = v; continue; }
        v = ExtractBoldValue(trimmed, "enabled");
        if (!v.empty()) { current->enabled = (v == "true"); continue; }
    }

    return agents;
}

std::string MarkdownParser::FormatAgents(const std::vector<Agent>& agents) {
    std::ostringstream out;
    out << "# Agents\n\n";
    for (const auto& a : agents) {
        out << "## " << a.id << "\n\n";
        out << "- **name**: " << a.name << "\n";
        if (!a.description.empty()) out << "- **description**: " << a.description << "\n";
        out << "- **command**: " << a.command << "\n";
        if (!a.args.empty()) out << "- **args**: " << a.args << "\n";
        if (!a.model.empty()) out << "- **model**: " << a.model << "\n";
        out << "- **timeout**: " << a.timeout << "\n";
        out << "- **auto_approve**: " << a.auto_approve << "\n";
        out << "- **enabled**: " << (a.enabled ? "true" : "false") << "\n";
        if (!a.tags.empty()) {
            out << "- **tags**: ";
            for (size_t i = 0; i < a.tags.size(); i++) {
                if (i > 0) out << ", ";
                out << a.tags[i];
            }
            out << "\n";
        }

        if (!a.instructions.empty()) {
            out << "\n### instructions\n\n";
            out << a.instructions << "\n";
        }

        if (!a.env.empty()) {
            out << "\n### env\n\n";
            for (const auto& [k, v] : a.env) {
                out << k << "=" << v << "\n";
            }
        }

        if (!a.skills.empty()) {
            out << "\n### skills\n\n";
            for (const auto& s : a.skills) {
                out << "- " << s << "\n";
            }
        }

        out << "\n---\n\n";
    }
    return out.str();
}

// ---- Skills ----

std::vector<Skill> MarkdownParser::ParseSkills(const std::string& content) {
    std::vector<Skill> skills;
    std::istringstream stream(content);
    std::string line;
    Skill* current = nullptr;
    bool in_instructions = false;

    while (std::getline(stream, line)) {
        auto trimmed = Trim(line);

        if (StartsWith(trimmed, "## ")) {
            skills.push_back(Skill());
            current = &skills.back();
            current->id = Trim(trimmed.substr(3));
            in_instructions = false;
            continue;
        }

        if (!current) continue;

        if (trimmed == "### instructions") {
            in_instructions = true;
            continue;
        }

        if (trimmed == "---") {
            current = nullptr;
            in_instructions = false;
            continue;
        }

        if (in_instructions) {
            if (!current->instructions.empty()) current->instructions += "\n";
            current->instructions += trimmed;
            continue;
        }

        auto v = ExtractBoldValue(trimmed, "description");
        if (!v.empty()) { current->description = v; continue; }
    }

    return skills;
}

std::string MarkdownParser::FormatSkills(const std::vector<Skill>& skills) {
    std::ostringstream out;
    out << "# Skills\n\n";
    for (const auto& s : skills) {
        out << "## " << s.id << "\n\n";
        out << "- **description**: " << s.description << "\n";
        if (!s.instructions.empty()) {
            out << "\n### instructions\n\n";
            out << s.instructions << "\n";
        }
        out << "\n---\n\n";
    }
    return out.str();
}

std::vector<Agent> MarkdownParser::MergeAgents(
    const std::vector<Agent>& global,
    const std::vector<Agent>& project)
{
    std::vector<Agent> result = global;
    for (const auto& pa : project) {
        bool found = false;
        for (auto& ga : result) {
            if (ga.id == pa.id) {
                // Merge: override non-empty fields
                if (!pa.name.empty()) ga.name = pa.name;
                if (!pa.description.empty()) ga.description = pa.description;
                if (!pa.command.empty()) ga.command = pa.command;
                if (!pa.args.empty()) ga.args = pa.args;
                if (!pa.model.empty()) ga.model = pa.model;
                if (pa.timeout != 300) ga.timeout = pa.timeout;
                if (!pa.auto_approve.empty()) ga.auto_approve = pa.auto_approve;
                ga.enabled = pa.enabled;
                if (!pa.instructions.empty()) ga.instructions = pa.instructions;
                // Env: merge (project overrides)
                for (const auto& [k, v] : pa.env) {
                    ga.env[k] = v;
                }
                // Skills: project replaces
                if (!pa.skills.empty()) ga.skills = pa.skills;
                // Tags: project replaces
                if (!pa.tags.empty()) ga.tags = pa.tags;
                found = true;
                break;
            }
        }
        if (!found) {
            result.push_back(pa);
        }
    }
    return result;
}
