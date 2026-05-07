#include "TaskManager.h"
#include "storage/MarkdownParser.h"
#include "storage/MarkdownWriter.h"
#include "core/FileSystem.h"
#include <sstream>
#include <ctime>
#include <iomanip>
#include <filesystem>

Task TaskManager::CreateTask(const std::string& project_path,
                              const std::string& title,
                              const std::string& description)
{
    Task task;
    task.id = GenerateTaskId({});
    task.title = title;
    task.description = description;
    task.status = "planning";
    task.created = Now();
    return task;
}

void TaskManager::UpdateTaskFile(const std::string& task_path, const Task& task) {
    auto content = MarkdownParser::FormatTask(task);
    FileSystem::WriteFile(task_path, content);
}

void TaskManager::DeleteTaskFile(const std::string& task_path) {
    std::error_code ec;
    std::filesystem::remove(task_path, ec);
}

Subtask TaskManager::CreateSubtask(const std::string& title,
                                    const std::string& description,
                                    const std::string& assignee)
{
    Subtask sub;
    sub.id = GenerateSubtaskId({});
    sub.title = title;
    sub.description = description;
    sub.assignee = assignee;
    sub.status = SubtaskStatus::Pending;
    sub.risk = RiskLevel::Low;
    sub.approval = ApprovalMode::Auto;
    return sub;
}

void TaskManager::UpdateSubtaskStatus(Task& task, const std::string& subtask_id,
                                       SubtaskStatus status, const std::string& result)
{
    for (auto& sub : task.subtasks) {
        if (sub.id == subtask_id) {
            sub.status = status;
            if (!result.empty()) sub.result = result;
            break;
        }
    }
}

void TaskManager::AddReply(Task& task, const std::string& subtask_id,
                            const std::string& sender, const std::string& timestamp,
                            const std::string& body)
{
    for (auto& sub : task.subtasks) {
        if (sub.id == subtask_id) {
            ConversationMsg msg;
            msg.sender = sender;
            msg.timestamp = timestamp;
            msg.body = body;
            msg.is_exec_log = false;
            sub.conversation.push_back(msg);
            break;
        }
    }
}

void TaskManager::AppendAudit(const std::string& audit_path, const std::string& timestamp,
                               const std::string& action, const std::string& actor,
                               const std::string& detail)
{
    auto entry = MarkdownWriter::FormatAuditEntry(timestamp, action, actor, detail);
    FileSystem::AppendFile(audit_path, entry);
}

std::string TaskManager::GenerateTaskId(const std::vector<Task>& existing) {
    int max_num = 0;
    for (const auto& t : existing) {
        auto dash = t.id.find('-');
        if (dash != std::string::npos) {
            try {
                int num = std::stoi(t.id.substr(0, dash));
                if (num > max_num) max_num = num;
            } catch (...) {}
        }
    }
    max_num++;
    std::ostringstream ss;
    ss << std::setw(3) << std::setfill('0') << max_num;
    return ss.str();
}

std::string TaskManager::GenerateSubtaskId(const std::vector<Subtask>& existing) {
    int max_num = 0;
    for (const auto& s : existing) {
        auto dash = s.id.find('-');
        if (dash != std::string::npos) {
            try {
                int num = std::stoi(s.id.substr(dash + 1));
                if (num > max_num) max_num = num;
            } catch (...) {}
        }
    }
    max_num++;
    std::ostringstream ss;
    ss << "ST-" << std::setw(3) << std::setfill('0') << max_num;
    return ss.str();
}

std::string TaskManager::Now() {
    auto t = std::time(nullptr);
    auto tm = std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%d %H:%M");
    return ss.str();
}
