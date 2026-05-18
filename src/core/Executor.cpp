#include "Executor.h"
#include "ProcessManager.h"
#include "TaskManager.h"
#include "SkillManager.h"
#include "ApprovalGate.h"
#include "core/FileSystem.h"
#include "storage/MarkdownParser.h"
#include "storage/MarkdownWriter.h"
#include <sstream>
#include <ctime>
#include <iomanip>
#include <cstdio>
#include <array>

#ifdef _MSC_VER
#define popen _popen
#define pclose _pclose
#endif

static std::string NowTimestamp() {
    auto t = std::time(nullptr);
    auto tm = std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

static std::string NowShort() {
    auto t = std::time(nullptr);
    auto tm = std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%d %H:%M");
    return ss.str();
}

Executor::Executor(ProcessManager& pm, TaskManager& tm, SkillManager* sm)
    : pm_(pm), tm_(tm), skill_mgr_(sm) {}

std::string Executor::NextExecutionId() {
    std::lock_guard<std::mutex> lock(mu_);
    int max_n = 0;
    for (const auto& e : executions_) {
        if (e.id.size() > 5) {
            try {
                int n = std::stoi(e.id.substr(5));
                if (n > max_n) max_n = n;
            } catch (...) {}
        }
    }
    std::ostringstream ss;
    ss << "EXEC-" << std::setw(3) << std::setfill('0') << (max_n + 1);
    return ss.str();
}

std::string Executor::GetGitDiff(const std::string& repo_path) {
    std::string git_dir = repo_path + "/.git";
    if (!FileSystem::DirExists(git_dir)) return "";

    std::array<char, 4096> buf;
    std::string result;
    std::string cmd = "cd /d \"" + repo_path + "\" 2>nul && git diff 2>nul";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    while (fgets(buf.data(), (int)buf.size(), pipe) != nullptr) {
        result += buf.data();
    }
    pclose(pipe);
    return result;
}

static std::string BuildPrompt(const Subtask& sub, const Agent& agent,
                                SkillManager* skill_mgr) {
    std::string prompt;
    if (!agent.instructions.empty()) {
        prompt += agent.instructions + "\n\n";
    }

    if (skill_mgr && !agent.skills.empty()) {
        for (const auto& skill_id : agent.skills) {
            prompt = skill_mgr->BuildPrompt(prompt, skill_id);
        }
    }

    if (!sub.description.empty()) {
        prompt += sub.description;
    }
    if (prompt.empty()) {
        prompt = sub.title;
    }
    return prompt;
}

void Executor::SpawnProcess(
    const Agent& agent,
    const std::string& prompt,
    const Project& project,
    std::shared_ptr<ExecPayload> payload)
{
    std::vector<std::string> args;
    if (!agent.args.empty()) {
        std::istringstream ss(agent.args);
        std::string a;
        while (ss >> a) {
            args.push_back(a);
        }
    }
    if (!prompt.empty()) {
        args.push_back(prompt);
    }

    std::string cwd = project.repo;
    std::map<std::string, std::string> env = agent.env;

    auto on_output = [this, payload](const std::string& pid, const std::string& data) {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& e : executions_) {
            if (e.id == payload->exec_id) {
                e.output += data;
                if (on_output_) on_output_(e);
                break;
            }
        }
    };

    auto on_exit = [this, payload](const std::string& pid, int code) {
        {
            std::lock_guard<std::mutex> lock(mu_);
            for (auto& e : executions_) {
                if (e.id == payload->exec_id) {
                    e.exit_code = code;
                    e.end_time = NowTimestamp();
                    e.status = (code == 0) ? Execution::Completed : Execution::Failed;
                    break;
                }
            }
        }

        // Track completed subtask for dependency resolution
        {
            std::lock_guard<std::mutex> lock(queue_mu_);
            completed_subtasks_.insert(payload->subtask_id);
        }
        ProcessQueue();

        auto task_files = FileSystem::ListFiles(payload->project_tasks_path, ".md");
        std::string found_path;
        for (const auto& f : task_files) {
            auto content = FileSystem::ReadFile(f);
            if (content.empty()) continue;
            auto parsed = MarkdownParser::ParseTask(content);
            if (parsed.id == payload->task_id) {
                found_path = f;
                break;
            }
        }

        if (!found_path.empty()) {
            auto content = FileSystem::ReadFile(found_path);
            std::string new_status = (code == 0) ? "completed" : "failed";
            std::string result_str = (code == 0)
                ? "exit:" + std::to_string(code)
                : "failed (exit:" + std::to_string(code) + ")";

            auto updated = MarkdownWriter::UpdateSubtaskStatus(
                content, payload->subtask_id, new_status, result_str);

            updated = MarkdownWriter::AddConversationMsg(
                updated, payload->subtask_id, payload->agent_id,
                NowShort(),
                (code == 0) ? "✅ 已完成，exit:" + std::to_string(code)
                            : "❌ 失败，exit:" + std::to_string(code),
                false);

            // Collect diff if successful
            if (code == 0 && on_diff_available_) {
                std::string diff = GetGitDiff(payload->project_repo);
                if (!diff.empty()) {
                    on_diff_available_(diff, payload->subtask_id);
                }
            }

            FileSystem::WriteFile(found_path, updated);

            tm_.AppendAudit(payload->project_audit_path, NowShort(),
                (code == 0) ? "subtask_complete" : "subtask_fail",
                payload->agent_id,
                payload->subtask_id + (code == 0 ? " completed" : " failed") +
                " (exit:" + std::to_string(code) + ")");
        }

        {
            std::lock_guard<std::mutex> lock(mu_);
            for (auto& e : executions_) {
                if (e.id == payload->exec_id) {
                    if (on_status_change_) on_status_change_(e);
                    break;
                }
            }
        }
    };

    auto pid = pm_.Spawn(agent.command, args, cwd, env, on_output, on_exit);

    {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& e : executions_) {
            if (e.id == payload->exec_id) {
                e.process_id = pid;
                break;
            }
        }
    }

    if (pid.empty()) {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& e : executions_) {
            if (e.id == payload->exec_id) {
                e.status = Execution::Failed;
                e.exit_code = -1;
                e.end_time = NowTimestamp();
                e.output += "\n[Error] Failed to spawn: " + agent.command + "\n";
                auto spawn_err = pm_.GetLastSpawnError();
                if (!spawn_err.empty()) {
                    e.output += "[Error] " + spawn_err + "\n";
                }
                if (on_status_change_) on_status_change_(e);
                break;
            }
        }
    }
}

void Executor::Execute(
    const Task& task,
    const std::string& subtask_id,
    const Agent& agent,
    const Project& project)
{
    const auto* sub = task.FindSubtask(subtask_id);
    if (!sub) return;

    // Approval gate check
    if (ApprovalGate::NeedsApproval(*sub, agent)) {
        auto task_paths = FileSystem::ListFiles(project.tasks_path, ".md");
        for (const auto& f : task_paths) {
            auto content = FileSystem::ReadFile(f);
            if (content.empty()) continue;
            auto parsed = MarkdownParser::ParseTask(content);
            if (parsed.id == task.id) {
                auto risk_str = ApprovalGate::RiskToString(
                    static_cast<ApprovalGate::RiskLevel>(sub->risk));
                auto updated = MarkdownWriter::UpdateSubtaskStatus(
                    content, subtask_id, "review",
                    std::string("Waiting for approval (risk: ") + risk_str + ")");
                FileSystem::WriteFile(f, updated);
                break;
            }
        }
        tm_.AppendAudit(project.AuditFilePath(), NowShort(),
            "subtask_execute", agent.id,
            subtask_id + " → " + agent.id + " (pending approval)");
        return;
    }

    Enqueue(task, subtask_id, agent, project);
}

void Executor::Enqueue(
    const Task& task,
    const std::string& subtask_id,
    const Agent& agent,
    const Project& project)
{
    {
        std::lock_guard<std::mutex> lock(queue_mu_);
        queue_.push_back({task, subtask_id, agent, project});
    }
    ProcessQueue();
}

void Executor::ProcessQueue() {
    std::lock_guard<std::mutex> lock(queue_mu_);
    for (auto it = queue_.begin(); it != queue_.end(); ) {
        if (CanExecute(it->task, it->subtask_id)) {
            bool blocked = false;
            if (HasSerialSubtaskRunning(it->task.id)) {
                auto* sub = it->task.FindSubtask(it->subtask_id);
                if (sub && sub->exec_mode == ExecMode::Serial) {
                    blocked = true;
                }
            }
            if (!blocked) {
                DoExecute(it->task, it->subtask_id, it->agent, it->project);
                it = queue_.erase(it);
                continue;
            }
        }
        ++it;
    }
}

bool Executor::CanExecute(const Task& task, const std::string& subtask_id) {
    auto* sub = task.FindSubtask(subtask_id);
    if (!sub) return false;

    for (const auto& dep : sub->depends) {
        if (completed_subtasks_.find(dep) == completed_subtasks_.end()) {
            return false;
        }
    }
    return true;
}

bool Executor::HasSerialSubtaskRunning(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& e : executions_) {
        if (e.task_id == task_id && e.status == Execution::Running) {
            return true;
        }
    }
    return false;
}

void Executor::DoExecute(
    const Task& task,
    const std::string& subtask_id,
    const Agent& agent,
    const Project& project)
{
    const auto* sub = task.FindSubtask(subtask_id);
    if (!sub) return;

    auto id = NextExecutionId();

    Execution exec;
    exec.id = id;
    exec.subtask_id = sub->id;
    exec.subtask_title = sub->title;
    exec.task_id = task.id;
    exec.task_title = task.title;
    exec.agent_id = agent.id;
    exec.agent_name = agent.name;
    exec.status = Execution::Pending;

    {
        std::lock_guard<std::mutex> lock(mu_);
        executions_.push_back(exec);
    }

    {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& e : executions_) {
            if (e.id == id) {
                e.start_time = NowTimestamp();
                e.status = Execution::Running;
                break;
            }
        }
    }

    auto payload = std::make_shared<ExecPayload>();
    payload->exec_id = id;
    payload->subtask_id = sub->id;
    payload->task_id = task.id;
    payload->task_title = task.title;
    payload->agent_id = agent.id;
    payload->project_name = project.name;
    payload->project_repo = project.repo;
    payload->project_tasks_path = project.tasks_path;
    payload->project_audit_path = project.AuditFilePath();
    payload->project_codeyoyo_path = project.codeyoyo_path;

    std::string prompt = BuildPrompt(*sub, agent, skill_mgr_);

    tm_.AppendAudit(project.AuditFilePath(), NowShort(), "subtask_execute",
        agent.id, sub->id + " → " + agent.id);

    SpawnProcess(agent, prompt, project, payload);
}

void Executor::Cancel(const std::string& execution_id) {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& e : executions_) {
        if (e.id == execution_id) {
            if (e.status == Execution::Running && !e.process_id.empty()) {
                pm_.Terminate(e.process_id);
            }
            e.status = Execution::Cancelled;
            e.end_time = NowTimestamp();
            if (on_status_change_) on_status_change_(e);
            break;
        }
    }
}

std::vector<Executor::Execution> Executor::GetExecutions() const {
    std::lock_guard<std::mutex> lock(mu_);
    return executions_;
}

bool Executor::IsBusy() const {
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& e : executions_) {
        if (e.status == Execution::Running) return true;
    }
    return false;
}