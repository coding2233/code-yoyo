#include "ProjectManager.h"
#include "storage/MarkdownParser.h"
#include "core/FileSystem.h"
#include <algorithm>

void ProjectManager::Init() {
    // Ensure directory structure exists
    FileSystem::CreateDirs(FileSystem::GetProjectsDir());
    LoadProjectsIndex();

    // Load each project's directory
    for (auto& p : projects_) {
        p.codeyoyo_path = FileSystem::GetProjectsDir() + "/" + p.name;
        p.tasks_path = p.codeyoyo_path + "/tasks";
        p.sessions_path = p.codeyoyo_path + "/sessions";
    }
}

void ProjectManager::Shutdown() {
    if (active_project_) {
        SaveProjectFile(*active_project_);
    }
    projects_.clear();
    active_project_ = nullptr;
}

void ProjectManager::LoadProjectsIndex() {
    auto path = FileSystem::GetProjectsDir() + "/../projects.md";
    if (!FileSystem::FileExists(path)) {
        FileSystem::WriteFile(path, "# Projects\n\n");
        return;
    }
    auto content = FileSystem::ReadFile(path);
    projects_ = MarkdownParser::ParseProjectsIndex(content);

    // Assign codeyoyo paths
    for (auto& p : projects_) {
        p.codeyoyo_path = FileSystem::GetProjectsDir() + "/" + p.name;
        p.tasks_path = p.codeyoyo_path + "/tasks";
        p.sessions_path = p.codeyoyo_path + "/sessions";
    }
}

void ProjectManager::SaveProjectsIndex() {
    auto path = FileSystem::GetProjectsDir() + "/../projects.md";
    auto content = MarkdownParser::FormatProjectsIndex(projects_);
    FileSystem::WriteFile(path, content);
}

void ProjectManager::SetActiveProject(const std::string& name) {
    for (auto& p : projects_) {
        if (p.name == name) {
            active_project_ = &p;
            RefreshProjectTasks(p);
            return;
        }
    }
    active_project_ = nullptr;
}

Project* ProjectManager::CreateProject(const std::string& name, const std::string& repo) {
    // Check for duplicate
    for (auto& p : projects_) {
        if (p.name == name) return nullptr;
    }

    Project p;
    p.name = name;
    p.repo = repo;
    p.created = TaskManager::Now();
    p.status = "active";
    p.codeyoyo_path = FileSystem::GetProjectsDir() + "/" + name;
    p.tasks_path = p.codeyoyo_path + "/tasks";
    p.sessions_path = p.codeyoyo_path + "/sessions";

    // Create directories
    FileSystem::CreateDirs(p.tasks_path);
    FileSystem::CreateDirs(p.sessions_path);

    // Write project.md
    SaveProjectFile(p);

    // Write agents.md (inherit from global if exists)
    auto agents_path = p.codeyoyo_path + "/agents.md";
    if (!FileSystem::FileExists(agents_path)) {
        FileSystem::WriteFile(agents_path, "# Agents\n\n");
    }

    // Write AUDIT.md
    FileSystem::WriteFile(p.AuditFilePath(), "# Audit Log\n\n");

    projects_.push_back(p);
    SaveProjectsIndex();
    active_project_ = &projects_.back();
    return active_project_;
}

void ProjectManager::RemoveProject(const std::string& name) {
    auto it = std::find_if(projects_.begin(), projects_.end(),
        [&](const Project& p) { return p.name == name; });
    if (it == projects_.end()) return;

    if (active_project_ && active_project_->name == name) {
        active_project_ = nullptr;
    }

    projects_.erase(it);
    SaveProjectsIndex();
}

void ProjectManager::RefreshProjectTasks(Project& p) {
    // This is a no-op for now; tasks are loaded on demand
}

void ProjectManager::SaveProjectFile(const Project& p) {
    auto content = MarkdownParser::FormatProject(p);
    FileSystem::WriteFile(p.ProjectFilePath(), content);
}
