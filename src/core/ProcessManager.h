#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

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

    void WriteStdin(const std::string& id, const std::string& data);
    void Terminate(const std::string& id);
    bool IsRunning(const std::string& id);
    void ShutdownAll();

private:
    struct ProcessInfo {
        std::string id;
#ifdef _WIN32
        HANDLE h_process = nullptr;
        HPCON h_pc = nullptr;
        HANDLE h_stdin_write = nullptr;
        HANDLE h_stdout_read = nullptr;
#endif
        std::thread worker;
        std::atomic<bool> finished{false};
        std::atomic<int> exit_code{-1};
        std::function<void(const std::string&, const std::string&)> on_output;
        std::function<void(const std::string&, int)> on_exit;
    };

    std::mutex mu_;
    std::map<std::string, std::unique_ptr<ProcessInfo>> processes_;
    int next_id_ = 0;

    std::string NextId();
    static void ReadThreadProc(ProcessInfo* info);
};
