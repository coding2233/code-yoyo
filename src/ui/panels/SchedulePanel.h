#pragma once

#include <string>
#include <vector>
#include <imgui.h>

class ProjectManager;
class TaskManager;
class LayoutManager;

class SchedulePanel {
public:
    void Render(ProjectManager& pm, TaskManager& tm, const LayoutManager& layout);
    void SetOpen(bool o) { open_ = o; }
    bool IsOpen() const { return open_; }

private:
    bool open_ = true;
};