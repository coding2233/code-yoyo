#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include "models/Task.h"
#include "models/Agent.h"
#include "models/Project.h"

class ProcessManager;
class TaskManager;
class SkillManager;

class Executor {
public:
    Executor(ProcessManager& pm, TaskManager& tm, SkillManager* sm = nullptr);

    struct Execution {
        std::string id;
        std::string process_id;
        std::string subtask_id;
        std::string subtask_title;
        std::string task_id;
        std::string task_title;
        std::string agent_id;
        std::string agent_name;

        enum Status {
            Pending,
            Running,
            Completed,
            Failed,
            Cancelled
        };
        Status status = Pending;
        int exit_code = -1;
        std::string output;

        std::string start_time;
        std::string end_time;
    };

    void Execute(
        const Task& task,
        const std::string& subtask_id,
        const Agent& agent,
        const Project& project
    );

    void Cancel(const std::string& execution_id);

    std::vector<Execution> GetExecutions() const;
    bool IsBusy() const;

    using ExecCallback = std::function<void(const Execution&)>;
    void SetOnOutput(ExecCallback cb) { on_output_ = std::move(cb); }
    void SetOnStatusChange(ExecCallback cb) { on_status_change_ = std::move(cb); }

    using DiffCallback = std::function<void(const std::string& diff, const std::string& subtask_id)>;
    void SetOnDiffAvailable(DiffCallback cb) { on_diff_available_ = std::move(cb); }

private:
    ProcessManager& pm_;
    TaskManager& tm_;
    SkillManager* skill_mgr_ = nullptr;

    mutable std::mutex mu_;
    std::vector<Execution> executions_;

    ExecCallback on_output_;
    ExecCallback on_status_change_;
    DiffCallback on_diff_available_;

    std::string NextExecutionId();
    std::string GetGitDiff(const std::string& repo_path);

    struct ExecPayload {
        std::string exec_id;
        std::string subtask_id;
        std::string task_id;
        std::string task_title;
        std::string agent_id;
        std::string project_name;
        std::string project_repo;
        std::string project_tasks_path;
        std::string project_audit_path;
        std::string project_codeyoyo_path;
    };

    void SpawnProcess(
        const Agent& agent,
        const std::string& prompt,
        const Project& project,
        std::shared_ptr<ExecPayload> payload
    );
};