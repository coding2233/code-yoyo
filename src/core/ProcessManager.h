#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

#include <reproc++/reproc.hpp>

class ProcessManager {
public:
    ProcessManager() = default;
    ~ProcessManager();

    std::string Spawn(
        const std::string& command,
        const std::vector<std::string>& args,
        const std::string& cwd,
        const std::map<std::string, std::string>& env,
        std::function<void(const std::string& id, const std::string& data)> on_output = nullptr,
        std::function<void(const std::string& id, int code)> on_exit = nullptr
    );

    std::string GetLastSpawnError() const { return last_spawn_error_; }

    void WriteStdin(const std::string& id, const std::string& data);
    void Terminate(const std::string& id);
    bool IsRunning(const std::string& id);
    void ShutdownAll();

private:
    struct ProcessInfo {
        std::string id;
        reproc::process proc;
        std::thread worker;
        std::atomic<bool> finished{false};
        std::atomic<int> exit_code{-1};
        std::function<void(const std::string&, const std::string&)> on_output;
        std::function<void(const std::string&, int)> on_exit;
    };

    std::mutex mu_;
    std::map<std::string, std::unique_ptr<ProcessInfo>> processes_;
    int next_id_ = 0;
    std::string last_spawn_error_;

    std::string NextId();
    void ReadThreadProc(ProcessInfo* info);
};
