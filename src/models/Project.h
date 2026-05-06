#pragma once

#include <string>
#include <vector>

struct Project {
    std::string id;
    std::string name;
    std::string repo;
    std::string created;
    std::string status = "active";

    // Derived: path to project files in ~/.codeyoyo/projects/<name>/
    std::string codeyoyo_path;
    std::string tasks_path;
    std::string sessions_path;

    std::string ProjectFilePath() const;
    std::string AuditFilePath() const;
    std::string TaskFilePath(const std::string& task_id, const std::string& title) const;
    std::string SessionFilePath(const std::string& task_id, const std::string& subtask_id) const;
};
