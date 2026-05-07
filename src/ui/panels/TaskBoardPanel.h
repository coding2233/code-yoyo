#pragma once

#include <string>
#include <vector>
#include <imgui.h>
#include "models/Task.h"

class TaskManager;
class ProjectManager;
class LayoutManager;

class TaskBoardPanel {
public:
    void Render(ProjectManager& pm, TaskManager& tm, const LayoutManager& layout);
    void SetOpen(bool o) { open_ = o; }
    bool IsOpen() const { return open_; }

    void SetSelectedSubtask(const std::string& task_id, const std::string& sub_id) {
        selected_task_id_ = task_id;
        selected_sub_id_ = sub_id;
    }
    std::string GetSelectedTaskId() const { return selected_task_id_; }
    std::string GetSelectedSubId() const { return selected_sub_id_; }
    bool HasSelection() const { return !selected_task_id_.empty() && !selected_sub_id_.empty(); }

private:
    bool open_ = true;
    std::string selected_task_id_;
    std::string selected_sub_id_;
    char new_task_title_[256] = {};
    bool show_new_task_popup_ = false;
};
