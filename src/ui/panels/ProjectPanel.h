#pragma once

#include <string>
#include <vector>
#include <imgui.h>

class ProjectManager;

class ProjectPanel {
public:
    void Render(ProjectManager& pm);
    void RenderNewProjectPopup(ProjectManager& pm);
    bool IsOpen() const { return open_; }
    void SetOpen(bool o) { open_ = o; }

private:
    bool open_ = true;
    char new_project_name_[128] = {};
    char new_project_repo_[256] = {};
    bool show_new_popup_ = false;
};
