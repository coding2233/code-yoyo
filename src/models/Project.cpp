#include "Project.h"
#include <algorithm>
#include <sstream>

std::string Project::ProjectFilePath() const {
    return codeyoyo_path + "/project.md";
}

std::string Project::AuditFilePath() const {
    return codeyoyo_path + "/AUDIT.md";
}

std::string Project::TaskFilePath(const std::string& task_id, const std::string& title) const {
    std::string safe_title = title;
    for (auto& c : safe_title) {
        if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '-';
        }
    }
    return tasks_path + "/" + task_id + "-" + safe_title + ".md";
}

std::string Project::SessionFilePath(const std::string& task_id, const std::string& subtask_id) const {
    return sessions_path + "/" + task_id + "/" + subtask_id + ".log";
}
