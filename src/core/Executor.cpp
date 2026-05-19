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
#include <iostream>

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
    } else if (!sub.title.empty()) {
        prompt += sub.title;
    }

    for (const auto& msg : sub.conversation) {
        prompt += "\n---\n[" + msg.sender + " (" + msg.timestamp + ")]\n" + msg.body;
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
    std::string resolved_args = agent.args;
    bool has_message_var = resolved_args.find("${message}") != std::string::npos;

    auto replace_var = [&](const std::string& var, const std::string& val) {
        size_t pos = 0;
        while ((pos = resolved_args.find(var, pos)) != std::string::npos) {
            resolved_args.replace(pos, var.size(), val);
            pos += val.size();
        }
    };
    replace_var("${model}", agent.model);
    replace_var("${instructions}", agent.instructions);
    replace_var("${message}", prompt);

    std::vector<std::string> args;
    if (!resolved_args.empty()) {
        std::istringstream ss(resolved_args);
        std::string a;
        while (ss >> a) {
            args.push_back(a);
        }
    }
    if (!has_message_var && !prompt.empty()) {
        args.push_back(prompt);
    }

    std::string cwd = project.repo;

    std::cerr << "[Executor] SpawnProcess: command=" << agent.command
              << ", cwd=" << cwd << "\n";
    std::cerr << "[Executor] SpawnProcess: full args:";
    // Reconstruct full arg list for logging
    std::vector<std::string> all_spawn_args;
    all_spawn_args.push_back(agent.command);
    for (const auto& a_ : args) all_spawn_args.push_back(a_);
    for (const auto& a_ : all_spawn_args) std::cerr << " '" << a_ << "'";
    std::cerr << "\n";

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
        std::string exec_output;
        {
            std::lock_guard<std::mutex> lock(mu_);
            for (auto& e : executions_) {
                if (e.id == payload->exec_id) {
                    e.exit_code = code;
                    e.end_time = NowTimestamp();
                    e.status = (code == 0) ? Execution::Completed : Execution::Failed;
                    exec_output = e.output;
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

            std::string conv_body;
            if (!exec_output.empty()) {
                conv_body = exec_output;
            } else {
                conv_body = (code == 0)
                    ? "exit:" + std::to_string(code)
                    : "failed (exit:" + std::to_string(code) + ")";
            }

            updated = MarkdownWriter::AddConversationMsg(
                updated, payload->subtask_id, payload->agent_id,
                NowShort(), conv_body, false);

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
        auto spawn_err = pm_.GetLastSpawnError();
        std::cerr << "[Executor] SpawnProcess FAILED: " << spawn_err << "\n";
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& e : executions_) {
            if (e.id == payload->exec_id) {
                e.status = Execution::Failed;
                e.exit_code = -1;
                e.end_time = NowTimestamp();
                e.output += "\n[Error] Failed to spawn: " + agent.command + "\n";
                if (!spawn_err.empty()) {
                    e.output += "[Error] " + spawn_err + "\n";
                }
                if (on_status_change_) on_status_change_(e);
                break;
            }
        }
    } else {
        std::cerr << "[Executor] SpawnProcess OK, pid=" << pid << "\n";
    }
}

void Executor::Execute(
    const Task& task,
    const std::string& subtask_id,
    const Agent& agent,
    const Project& project)
{
    const auto* sub = task.FindSubtask(subtask_id);
    if (!sub) {
        std::cerr << "[Executor] Execute: subtask '" << subtask_id << "' not found in task '" << task.id << "'\n";
        return;
    }

    std::cerr << "[Executor] Execute: task=" << task.id << ", sub=" << subtask_id
              << ", agent=" << agent.id << ", auto_approve=" << agent.auto_approve
              << ", sub_risk=" << static_cast<int>(sub->risk) << "\n";

    // Approval gate check
    if (ApprovalGate::NeedsApproval(*sub, agent)) {
        std::cerr << "[Executor] Approval needed (risk exceeds threshold)\n";
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
                std::cerr << "[Executor] Status set to review (pending approval)\n";
                break;
            }
        }
        tm_.AppendAudit(project.AuditFilePath(), NowShort(),
            "subtask_execute", agent.id,
            subtask_id + " → " + agent.id + " (pending approval)");
        return;
    }

    std::cerr << "[Executor] No approval needed, enqueuing\n";
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
    std::cerr << "[Executor] ProcessQueue: queue size=" << queue_.size() << "\n";
    for (auto it = queue_.begin(); it != queue_.end(); ) {
        bool can_exec = CanExecute(it->task, it->subtask_id);
        std::cerr << "[Executor] ProcessQueue: sub=" << it->subtask_id
                  << ", can_exec=" << can_exec << "\n";
        if (can_exec) {
            bool blocked = false;
            if (HasSerialSubtaskRunning(it->task.id)) {
                auto* sub = it->task.FindSubtask(it->subtask_id);
                if (sub && sub->exec_mode == ExecMode::Serial) {
                    blocked = true;
                    std::cerr << "[Executor] ProcessQueue: blocked by serial exec\n";
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
    if (!sub) {
        std::cerr << "[Executor] DoExecute: subtask '" << subtask_id << "' not found\n";
        return;
    }
    std::cerr << "[Executor] DoExecute: sub=" << subtask_id << ", agent=" << agent.id << "\n";

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