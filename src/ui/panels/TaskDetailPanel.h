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
    void SetTask(Task* t) { current_task_ = t; }
    void SetSubtask(Subtask* s) { current_subtask_ = s; }

private:
    void RenderSubtaskDetail(const Task& task, Subtask& sub,
        Executor& exec, AgentManager& agent_mgr, const Project& project);
    void RenderConversation(const Subtask& sub);
    void ShowAgentAutocomplete(const std::string& partial, AgentManager& agent_mgr);
    void HandleAgentTrigger(const std::string& body, Subtask& sub, const Task& task,
        Executor& exec, AgentManager& agent_mgr, const Project& project);
    void TriggerDecompose(Task& task, const Project& project,
        Executor& exec, AgentManager& agent_mgr, TaskManager& tm);
    static std::vector<Subtask> DecomposeTaskLocal(const std::string& description);

    bool open_ = true;
    Task* current_task_ = nullptr;
    Subtask* current_subtask_ = nullptr;
    char reply_input_[4096] = {};
    char agent_buf_[64] = {};

    bool show_autocomplete_ = false;
    int autocomplete_cursor_ = 0;
    bool show_decompose_popup_ = false;
};