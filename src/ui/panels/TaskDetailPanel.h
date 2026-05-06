#pragma once

#include <string>
#include <imgui.h>
#include "models/Task.h"

class TaskManager;
class ProjectManager;

class TaskDetailPanel {
public:
    void Render(ProjectManager& pm, TaskManager& tm);
    void SetOpen(bool o) { open_ = o; }
    bool IsOpen() const { return open_; }
    void SetTask(Task* t) { current_task_ = t; }
    void SetSubtask(Subtask* s) { current_subtask_ = s; }

private:
    void RenderSubtaskDetail(const Task& task, Subtask& sub);
    void RenderConversation(const Subtask& sub);

    bool open_ = true;
    Task* current_task_ = nullptr;
    Subtask* current_subtask_ = nullptr;
    char reply_input_[4096] = {};
};
