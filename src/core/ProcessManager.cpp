#include "ProcessManager.h"
#include <iostream>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#endif
namespace fs = std::filesystem;

ProcessManager::~ProcessManager() {
    ShutdownAll();
}

std::string ProcessManager::NextId() {
    return "P" + std::to_string(++next_id_);
}

std::string ProcessManager::Spawn(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::string& cwd,
    const std::map<std::string, std::string>& env,
    std::function<void(const std::string& id, const std::string& data)> on_output,
    std::function<void(const std::string& id, int code)> on_exit)
{
    last_spawn_error_.clear();
    auto id = NextId();
    auto info = std::make_unique<ProcessInfo>();
    info->id = id;
    info->on_output = std::move(on_output);
    info->on_exit = std::move(on_exit);

    std::vector<std::string> all_args;

#ifdef _WIN32
    std::string resolved_cmd = command;
    bool no_ext = command.find('.') == std::string::npos;
    if (no_ext) {
        bool found_cmd = false;
        char buf[MAX_PATH];
        char* ext;
        DWORD ret = SearchPathA(nullptr, command.c_str(), ".cmd", MAX_PATH, buf, &ext);
        std::cerr << "[ProcessManager] SearchPathA('" << command << "', '.cmd') returned " << ret << "\n";
        if (ret > 0) {
            found_cmd = true;
            std::cerr << "[ProcessManager]  found at: " << buf << "\n";
        }

        if (found_cmd) {
            all_args.push_back("cmd.exe");
            all_args.push_back("/c");
            all_args.push_back(command);
            for (const auto& a : args) {
                all_args.push_back(a);
            }
        } else {
            all_args.push_back(command);
            for (const auto& a : args) {
                all_args.push_back(a);
            }
        }
    } else {
        all_args.push_back(command);
        for (const auto& a : args) {
            all_args.push_back(a);
        }
    }
#else
    all_args.push_back(command);
    for (const auto& a : args) {
        all_args.push_back(a);
    }
#endif

    reproc::options opts;
    if (!cwd.empty()) {
        std::error_code ec2;
        bool cwd_ok = fs::exists(cwd, ec2);
        std::cerr << "[ProcessManager] cwd='" << cwd << "' exists=" << cwd_ok;
        if (!cwd_ok) std::cerr << " error=" << ec2.message();
        std::cerr << "\n";
        // Print raw hex for debugging encoding
        std::cerr << "[ProcessManager] cwd hex:";
        for (unsigned char cc : cwd) std::cerr << ' ' << std::hex << (int)cc;
        std::cerr << std::dec << "\n";
        if (cwd_ok) {
            opts.working_directory = cwd.c_str();
        }
    }
    if (!env.empty()) {
        opts.env.behavior = reproc::env::extend;
        opts.env.extra = reproc::env(env);
    }
    opts.redirect.in = { reproc::redirect::pipe };
    opts.redirect.out = { reproc::redirect::pipe };
    opts.redirect.err = { reproc::redirect::stdout_ };

    std::cerr << "[ProcessManager] actual spawn args:";
    for (const auto& a_ : all_args) std::cerr << " '" << a_ << "'";
    std::cerr << "\n";

    auto ec = info->proc.start(all_args, opts);
    if (ec) {
        std::cerr << "[ProcessManager] start failed: " << ec.message() << "\n";
        last_spawn_error_ = "Failed to spawn: " + command + " (" + ec.message() + ")";
        return "";
    }

    info->worker = std::thread(&ProcessManager::ReadThreadProc, this, info.get());

    {
        std::lock_guard<std::mutex> lock(mu_);
        processes_[id] = std::move(info);
    }
    return id;
}

void ProcessManager::WriteStdin(const std::string& id, const std::string& data) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = processes_.find(id);
    if (it == processes_.end()) return;

    it->second->proc.write(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
}

void ProcessManager::Terminate(const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = processes_.find(id);
    if (it == processes_.end()) return;

    it->second->proc.terminate();
}

bool ProcessManager::IsRunning(const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = processes_.find(id);
    if (it == processes_.end()) return false;
    return !it->second->finished;
}

void ProcessManager::ShutdownAll() {
    std::vector<std::thread> threads;
    {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& [id, info] : processes_) {
            if (!info->finished) {
                info->proc.terminate();
                info->proc.kill();
            }
            if (info->worker.joinable()) {
                threads.push_back(std::move(info->worker));
            }
        }
    }
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
    {
        std::lock_guard<std::mutex> lock(mu_);
        processes_.clear();
    }
}

void ProcessManager::ReadThreadProc(ProcessInfo* info) {
    uint8_t buf[4096];

    while (true) {
        auto [bytes_read, ec] = info->proc.read(reproc::stream::out, buf, sizeof(buf) - 1);
        if (ec) {
            break;
        }
        if (bytes_read > 0) {
            buf[bytes_read] = '\0';
            if (info->on_output) {
                info->on_output(info->id, std::string(reinterpret_cast<char*>(buf), bytes_read));
            }
        }
    }

    auto [status, ec] = info->proc.wait(reproc::milliseconds(0));
    if (ec) {
        info->proc.terminate();
        std::tie(status, ec) = info->proc.wait(reproc::milliseconds(5000));
    }

    info->exit_code = status;
    info->finished = true;
    if (info->on_exit) {
        info->on_exit(info->id, status);
    }
}
