#pragma once

#include <string>
#include <vector>
#include "models/Task.h"

class TaskManager {
public:
    // CRUD
    Task CreateTask(const std::string& project_path,
                    const std::string& title,
                    const std::string& description);
    void UpdateTaskFile(const std::string& task_path, const Task& task);
    void DeleteTaskFile(const std::string& task_path);

    // Subtask operations
    Subtask CreateSubtask(const std::string& title,
                          const std::string& description,
                          const std::string& assignee = "");

    // Status helpers
    void UpdateSubtaskStatus(Task& task, const std::string& subtask_id,
                              SubtaskStatus status, const std::string& result = "");

    // Conversation
    void AddReply(Task& task, const std::string& subtask_id,
                  const std::string& sender, const std::string& timestamp,
                  const std::string& body);

    // Audit
    void AppendAudit(const std::string& audit_path, const std::string& timestamp,
                     const std::string& action, const std::string& actor,
                     const std::string& detail);

    // Generate task ID
    static std::string GenerateTaskId(const std::vector<Task>& existing);
    static std::string GenerateSubtaskId(const std::vector<Subtask>& existing);
    static std::string Now();
};
