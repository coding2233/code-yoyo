#include "ProcessManager.h"
#include <algorithm>
#include <sstream>
#include <cstring>

ProcessManager::~ProcessManager() {
    ShutdownAll();
}

std::string ProcessManager::NextId() {
    return "P" + std::to_string(++next_id_);
}

static std::wstring ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], len);
    return w;
}

static std::string BuildCommandLine(const std::string& command, const std::vector<std::string>& args) {
    auto quote_if_needed = [](const std::string& s) -> std::string {
        if (s.find(' ') != std::string::npos || s.find('\t') != std::string::npos) {
            return "\"" + s + "\"";
        }
        return s;
    };

    std::string cmd = quote_if_needed(command);
    for (const auto& a : args) {
        cmd += " " + quote_if_needed(a);
    }
    return cmd;
}

static void* CreateEnvironmentBlock(const std::map<std::string, std::string>& env) {
    std::wstring block;
    for (const auto& [k, v] : env) {
        auto wk = ToWide(k);
        auto wv = ToWide(v);
        block.append(wk).append(L"=").append(wv).push_back(L'\0');
    }
    block.push_back(L'\0');
    if (block.empty()) return nullptr;

    size_t bytes = block.size() * sizeof(wchar_t);
    void* buf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
    if (buf) memcpy(buf, block.data(), bytes);
    return buf;
}

std::string ProcessManager::Spawn(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::string& cwd,
    const std::map<std::string, std::string>& env,
    std::function<void(const std::string& id, const std::string& data)> on_output,
    std::function<void(const std::string& id, int code)> on_exit)
{
    auto id = NextId();

    auto info = std::make_unique<ProcessInfo>();
    info->id = id;
    info->on_output = std::move(on_output);
    info->on_exit = std::move(on_exit);

    // Create pipes for ConPTY
    HANDLE h_input_write = nullptr;
    HANDLE h_input_read = nullptr;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };

    if (!CreatePipe(&h_input_read, &h_input_write, &sa, 0)) {
        return "";
    }

    HANDLE h_output_write = nullptr;
    HANDLE h_output_read = nullptr;
    if (!CreatePipe(&h_output_read, &h_output_write, &sa, 0)) {
        CloseHandle(h_input_read);
        CloseHandle(h_input_write);
        return "";
    }

    // Create pseudo console
    HPCON h_pc = nullptr;
    COORD console_size = { 120, 4096 };
    HRESULT hr = CreatePseudoConsole(console_size, h_input_read, h_output_write, 0, &h_pc);
    if (FAILED(hr)) {
        CloseHandle(h_input_read);
        CloseHandle(h_input_write);
        CloseHandle(h_output_read);
        CloseHandle(h_output_write);
        return "";
    }

    info->h_pc = h_pc;
    info->h_stdin_write = h_input_write;
    info->h_stdout_read = h_output_read;

    // Close the ends owned by ConPTY
    CloseHandle(h_input_read);
    CloseHandle(h_output_write);

    // Build command line
    auto cmd_line = BuildCommandLine(command, args);
    auto wcmd = ToWide(cmd_line);

    // Build environment block
    void* env_block = nullptr;
    if (!env.empty()) {
        env_block = CreateEnvironmentBlock(env);
    }

    // Convert cwd to wide
    std::wstring wcwd;
    if (!cwd.empty()) {
        wcwd = ToWide(cwd);
    }

    // Prepare STARTUPINFOEX
    SIZE_T attr_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
    auto attr_list = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, attr_size);
    if (!attr_list) {
        ClosePseudoConsole(h_pc);
        CloseHandle(h_input_write);
        CloseHandle(h_output_read);
        if (env_block) HeapFree(GetProcessHeap(), 0, env_block);
        return "";
    }
    InitializeProcThreadAttributeList(attr_list, 1, 0, &attr_size);
    UpdateProcThreadAttribute(attr_list, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, h_pc, sizeof(HPCON), nullptr, nullptr);

    STARTUPINFOEX si = {0};
    si.StartupInfo.cb = sizeof(STARTUPINFOEX);
    si.lpAttributeList = attr_list;

    PROCESS_INFORMATION pi = {0};
    BOOL created = CreateProcessW(
        nullptr,
        &wcmd[0],
        nullptr,
        nullptr,
        FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        env_block,
        wcwd.empty() ? nullptr : wcwd.c_str(),
        &si.StartupInfo,
        &pi
    );

    // Cleanup attribute list
    DeleteProcThreadAttributeList(attr_list);
    HeapFree(GetProcessHeap(), 0, attr_list);

    if (env_block) {
        HeapFree(GetProcessHeap(), 0, env_block);
    }

    if (!created) {
        ClosePseudoConsole(h_pc);
        CloseHandle(h_input_write);
        CloseHandle(h_output_read);
        return "";
    }

    info->h_process = pi.hProcess;
    CloseHandle(pi.hThread);

    // Start reader thread
    info->worker = std::thread(ReadThreadProc, info.get());

    // Store
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

    auto& info = it->second;
    DWORD written = 0;
    WriteFile(info->h_stdin_write, data.c_str(), (DWORD)data.size(), &written, nullptr);
}

void ProcessManager::Terminate(const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = processes_.find(id);
    if (it == processes_.end()) return;

    auto& info = it->second;
    if (info->h_process) {
        TerminateProcess(info->h_process, 1);
    }
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
            if (!info->finished && info->h_process) {
                TerminateProcess(info->h_process, 1);
            }
            if (info->worker.joinable()) {
                threads.push_back(std::move(info->worker));
            }
        }
    }
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
    std::lock_guard<std::mutex> lock(mu_);
    processes_.clear();
}

void ProcessManager::ReadThreadProc(ProcessInfo* info) {
    char buf[4096];
    DWORD read = 0;

    // Read output until the process exits
    while (ReadFile(info->h_stdout_read, buf, sizeof(buf) - 1, &read, nullptr) && read > 0) {
        buf[read] = '\0';
        if (info->on_output) {
            info->on_output(info->id, std::string(buf, read));
        }
    }

    // Wait for process to exit
    DWORD wait_result = WaitForSingleObject(info->h_process, INFINITE);
    DWORD code = 0;
    if (wait_result == WAIT_OBJECT_0) {
        GetExitCodeProcess(info->h_process, &code);
    }
    info->exit_code = (int)code;
    info->finished = true;

    if (info->on_exit) {
        info->on_exit(info->id, (int)code);
    }

    // Cleanup handles
    if (info->h_pc) {
        ClosePseudoConsole(info->h_pc);
        info->h_pc = nullptr;
    }
    if (info->h_stdin_write) {
        CloseHandle(info->h_stdin_write);
        info->h_stdin_write = nullptr;
    }
    if (info->h_stdout_read) {
        CloseHandle(info->h_stdout_read);
        info->h_stdout_read = nullptr;
    }
    if (info->h_process) {
        CloseHandle(info->h_process);
        info->h_process = nullptr;
    }
}
