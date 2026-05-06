#pragma once

#include <string>
#include <vector>
#include "models/Project.h"
#include "models/Task.h"

class ProjectManager {
public:
    void Init();
    void Shutdown();

    std::vector<Project>& GetProjects() { return projects_; }
    Project* GetActiveProject() { return active_project_; }
    void SetActiveProject(const std::string& name);

    Project* CreateProject(const std::string& name, const std::string& repo);
    void RemoveProject(const std::string& name);

    void RefreshProjectTasks(Project& p);
    void SaveProjectFile(const Project& p);

private:
    void LoadProjectsIndex();
    void SaveProjectsIndex();
    void LoadProject(const std::string& name);

    std::vector<Project> projects_;
    Project* active_project_ = nullptr;
};
