#include "Scheduler.h"
#include "core/ProjectManager.h"
#include "core/TaskManager.h"
#include "core/Executor.h"
#include "core/AgentManager.h"
#include "core/FileSystem.h"
#include "storage/MarkdownParser.h"
#include "storage/MarkdownWriter.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

Scheduler::Scheduler(ProjectManager& pm, TaskManager& tm, Executor& exec, AgentManager& agent_mgr)
    : pm_(pm), tm_(tm), exec_(exec), agent_mgr_(agent_mgr) {}

Scheduler::~Scheduler() {
    Stop();
}

void Scheduler::Start() {
    if (running_) return;
    running_ = true;
    thread_ = std::make_unique<std::thread>(&Scheduler::Loop, this);
}

void Scheduler::Stop() {
    running_ = false;
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

void Scheduler::SetInterval(int seconds) {
    check_interval_ = seconds > 0 ? seconds : 30;
}

void Scheduler::Loop() {
    while (running_) {
        CheckAndTrigger();
        std::this_thread::sleep_for(std::chrono::seconds(check_interval_));
    }
}

void Scheduler::ParseCron(const std::string& cron_expr, int& minute, int& hour, int& dom, int& month, int& dow) {
    std::istringstream ss(cron_expr);
    std::string parts[5];
    for (int i = 0; i < 5; i++) {
        if (!(ss >> parts[i])) {
            parts[i] = "*";
        }
    }
    auto parse_field = [](const std::string& s, int def) -> int {
        if (s == "*") return -1;
        try { return std::stoi(s); } catch (...) { return def; }
    };
    minute = parse_field(parts[0], 0);
    hour = parse_field(parts[1], 0);
    dom = parse_field(parts[2], -1);
    month = parse_field(parts[3], -1);
    dow = parse_field(parts[4], -1);
}

bool Scheduler::MatchesCron(const std::string& cron_expr, std::time_t t) {
    auto* tm = std::localtime(&t);
    if (!tm) return false;

    int cmin, chour, cdom, cmonth, cdow;
    ParseCron(cron_expr, cmin, chour, cdom, cmonth, cdow);

    if (cmin >= 0 && cmin != tm->tm_min) return false;
    if (chour >= 0 && chour != tm->tm_hour) return false;
    if (cdom >= 0 && cdom != tm->tm_mday) return false;
    if (cmonth >= 0 && cmonth != (tm->tm_mon + 1)) return false;
    if (cdow >= 0 && cdow != tm->tm_wday) return false;
    return true;
}

std::string Scheduler::FormatTime(std::time_t t) {
    auto* tm = std::localtime(&t);
    if (!tm) return "1970-01-01 00:00";
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%d %H:%M");
    return ss.str();
}

std::time_t Scheduler::ParseTime(const std::string& s) {
    std::tm tm = {};
    std::istringstream ss(s);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M");
    if (ss.fail()) return 0;
    return std::mktime(&tm);
}

void Scheduler::CheckAndTrigger() {
    auto now = std::time(nullptr);

    auto& projects = pm_.GetProjects();
    for (auto& project : projects) {
        if (project.status != "active") continue;

        auto task_files = FileSystem::ListFiles(project.tasks_path, ".md");
        for (const auto& task_path : task_files) {
            auto content = FileSystem::ReadFile(task_path);
            if (content.empty()) continue;

            auto task = MarkdownParser::ParseTask(content);
            if (task.status != "active" || task.schedule.empty()) continue;

            // Check if task has an assignee for auto-execution
            if (task.assignee.empty()) continue;

            // Parse next_run
            std::time_t next_run = 0;
            if (!task.next_run.empty()) {
                next_run = ParseTime(task.next_run);
            }

            // If no next_run set or time is due
            if (next_run == 0 || now >= next_run) {
                if (!MatchesCron(task.schedule, now)) continue;

                // Trigger execution
                auto* agent = agent_mgr_.FindAgent(task.assignee);
                if (!agent || !agent->enabled) continue;

                // Execute first pending subtask
                for (auto& sub : task.subtasks) {
                    if (sub.status == SubtaskStatus::Pending) {
                        // Update last_run and next_run
                        std::string last_run_str = FormatTime(now);
                        std::string next_run_str = FormatTime(now + 86400); // +1 day fallback
                        if (MatchesCron(task.schedule, now + 60)) {
                            next_run_str = FormatTime(now + 60);
                        } else {
                            // Simple next-run estimation: add 1 day for daily, 1 hour for hourly
                            int cmin, chour, cdom, cmonth, cdow;
                            ParseCron(task.schedule, cmin, chour, cdom, cmonth, cdow);
                            if (chour >= 0 && cdom < 0) {
                                // Daily at specific hour
                                auto next = now + 86400;
                                next_run_str = FormatTime(next);
                            } else if (cmin >= 0 && chour < 0) {
                                auto next = now + 3600;
                                next_run_str = FormatTime(next);
                            } else {
                                auto next = now + 86400;
                                next_run_str = FormatTime(next);
                            }
                        }

                        auto updated = MarkdownWriter::UpdateSchedule(content, last_run_str, next_run_str);
                        FileSystem::WriteFile(task_path, updated);

                        // Write audit
                        tm_.AppendAudit(project.AuditFilePath(), FormatTime(now),
                            "schedule_trigger", "scheduler",
                            task.id + " triggered by schedule " + task.schedule);

                        // Execute the subtask
                        exec_.Execute(task, sub.id, *agent, project);
                        break;
                    }
                }
            }
        }
    }
}