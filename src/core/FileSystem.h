#pragma once

#include <string>
#include <vector>

namespace FileSystem {

bool FileExists(const std::string& path);
bool DirExists(const std::string& path);
bool CreateDir(const std::string& path);
bool CreateDirs(const std::string& path);
std::string ReadFile(const std::string& path);
bool WriteFile(const std::string& path, const std::string& content);
bool AppendFile(const std::string& path, const std::string& content);
std::vector<std::string> ListFiles(const std::string& dir, const std::string& ext = "");
std::string GetHomeDir();
std::string GetCodeYoYoDir();
std::string GetProjectsDir();

}
