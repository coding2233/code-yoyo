#pragma once

#include <string>
#include <vector>
#include <imgui.h>
#include "models/Task.h"

class ProjectManager;
class LayoutManager;

class TaskTreePanel {
public:
    void Render(class ProjectManager& pm, const LayoutManager& layout);
    void SetOpen(bool o) { open_ = o; }
    bool IsOpen() const { return open_; }

private:
    bool open_ = true;
    int selected_task_idx_ = -1;
    int selected_sub_idx_ = -1;
};