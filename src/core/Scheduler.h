#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>
#include <mutex>
#include <memory>
#include <ctime>

class ProjectManager;
class TaskManager;
class Executor;
class AgentManager;

class Scheduler {
public:
    Scheduler(ProjectManager& pm, TaskManager& tm, Executor& exec, AgentManager& agent_mgr);
    ~Scheduler();

    void Start();
    void Stop();
    void SetInterval(int seconds);

    bool IsRunning() const { return running_; }

private:
    ProjectManager& pm_;
    TaskManager& tm_;
    Executor& exec_;
    AgentManager& agent_mgr_;

    std::unique_ptr<std::thread> thread_;
    std::atomic<bool> running_{false};
    int check_interval_ = 30; // seconds

    void Loop();
    void CheckAndTrigger();
    void ParseCron(const std::string& cron_expr, int& minute, int& hour, int& dom, int& month, int& dow);
    bool MatchesCron(const std::string& cron_expr, std::time_t t);
    std::string FormatTime(std::time_t t);
    std::time_t ParseTime(const std::string& s);
};