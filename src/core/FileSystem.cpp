#include "FileSystem.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#endif
namespace fs = std::filesystem;

namespace FileSystem {

bool FileExists(const std::string& path) {
    return fs::exists(path) && fs::is_regular_file(path);
}

bool DirExists(const std::string& path) {
    return fs::exists(path) && fs::is_directory(path);
}

bool CreateDir(const std::string& path) {
    return fs::create_directory(path);
}

bool CreateDirs(const std::string& path) {
    return fs::create_directories(path);
}

std::string ReadFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool WriteFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << content;
    return f.good();
}

bool AppendFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::app);
    if (!f.is_open()) return false;
    f << content;
    return f.good();
}

std::vector<std::string> ListFiles(const std::string& dir, const std::string& ext) {
    std::vector<std::string> files;
    if (!DirExists(dir)) return files;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            auto path = entry.path().string();
            if (ext.empty() || path.size() >= ext.size() &&
                path.compare(path.size() - ext.size(), ext.size(), ext) == 0) {
                files.push_back(path);
            }
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::string GetHomeDir() {
    auto home = std::getenv("USERPROFILE");
    if (home) return std::string(home);
    home = std::getenv("HOME");
    if (home) return std::string(home);
    return ".";
}

std::string GetCodeYoYoDir() {
    return GetHomeDir() + "/.code-yoyo";
}

std::string GetProjectsDir() {
    return GetCodeYoYoDir() + "/projects";
}

std::string GetExeDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string path(buf);
    auto pos = path.find_last_of("/\\");
    return (pos != std::string::npos) ? path.substr(0, pos) : ".";
#else
    return ".";
#endif
}

std::string SanitizePath(const std::string& path) {
    std::string result;
    result.reserve(path.size());
    for (unsigned char c : path) {
        // Keep printable ASCII, common path chars, and valid UTF-8 continuations
        if (c >= 0x20 && c <= 0x7E) {
            result += static_cast<char>(c);
        } else if (c >= 0x80) {
            // Keep valid UTF-8 multi-byte sequences, but drop U+FFFD (EF BF BD)
            if (c == 0xEF) {
                // Check for U+FFFD (EF BF BD)
                auto idx = &c - reinterpret_cast<const unsigned char*>(path.data());
                if (idx + 2 < path.size() &&
                    static_cast<unsigned char>(path[idx + 1]) == 0xBF &&
                    static_cast<unsigned char>(path[idx + 2]) == 0xBD) {
                    // Skip this replacement character sequence
                    continue;
                }
            }
            // Also drop standalone byte 0x80-0xBF (invalid start)
            if (c < 0xC0) continue;
            result += static_cast<char>(c);
        }
        // Drop control characters (0x00-0x1F)
    }
    // Trim trailing space/dot
    while (!result.empty() && (result.back() == ' ' || result.back() == '.'))
        result.pop_back();
    return result;
}

}
