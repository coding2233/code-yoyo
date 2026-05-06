#include "FileSystem.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
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
    return GetHomeDir() + "/.codeyoyo";
}

std::string GetProjectsDir() {
    return GetCodeYoYoDir() + "/projects";
}

}
