#pragma once

#include <string>
#include <vector>
#include <imgui.h>
#include "models/Task.h"

class TaskManager;
class ProjectManager;

class TaskBoardPanel {
public:
    void Render(ProjectManager& pm, TaskManager& tm);
    void SetOpen(bool o) { open_ = o; }
    bool IsOpen() const { return open_; }
    void SetSelectedSubtask(Subtask* s) { selected_subtask_ = s; }
    Subtask* GetSelectedSubtask() const { return selected_subtask_; }

private:
    void RenderKanbanColumn(const std::string& title, const std::string& status,
                            const std::vector<Subtask>& subtasks,
                            SubtaskStatus filter_status);
    void RenderCard(const Subtask& sub);

    bool open_ = true;
    Subtask* selected_subtask_ = nullptr;
    char new_task_title_[256] = {};
    bool show_new_task_popup_ = false;
};
