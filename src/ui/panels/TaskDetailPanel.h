#pragma once

#include <string>
#include <vector>
#include <imgui.h>
#include "models/Task.h"

class TaskManager;
class ProjectManager;
class Executor;
class AgentManager;
struct Project;
class LayoutManager;

class TaskDetailPanel {
public:
    void Render(ProjectManager& pm, TaskManager& tm, Executor& exec, AgentManager& agent_mgr, const LayoutManager& layout);
    void SetOpen(bool o) { open_ = o; }
    bool IsOpen() const { return open_; }

    void SetTask(const Task& t, int subtask_idx) {
        current_task_ = t;
        current_subtask_idx_ = (subtask_idx >= 0 && subtask_idx < (int)t.subtasks.size()) ? subtask_idx : -1;
    }
    bool HasSelection() const { return current_subtask_idx_ >= 0 && !current_task_.id.empty(); }
    Task* GetTask() { return current_task_.id.empty() ? nullptr : &current_task_; }
    Subtask* GetSubtask() {
        if (current_subtask_idx_ < 0 || current_subtask_idx_ >= (int)current_task_.subtasks.size()) return nullptr;
        return &current_task_.subtasks[current_subtask_idx_];
    }

private:
    void RenderSubtaskDetail(Task& task, Subtask& sub,
        Executor& exec, AgentManager& agent_mgr, const Project& project);
    void RenderConversation(const Subtask& sub);
    void ShowAgentAutocomplete(const std::string& partial, AgentManager& agent_mgr);
    void HandleAgentTrigger(const std::string& body, Subtask& sub, const Task& task,
        Executor& exec, AgentManager& agent_mgr, const Project& project);
    void TriggerDecompose(Task& task, const Project& project,
        Executor& exec, AgentManager& agent_mgr, TaskManager& tm);
    static std::vector<Subtask> DecomposeTaskLocal(const std::string& description);

    bool open_ = true;
    Task current_task_;
    int current_subtask_idx_ = -1;
    char reply_input_[4096] = {};
    char agent_buf_[64] = {};

    bool show_autocomplete_ = false;
    int autocomplete_cursor_ = 0;
    bool show_decompose_popup_ = false;
};
