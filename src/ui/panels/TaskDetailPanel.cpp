#include "TaskDetailPanel.h"
#include "core/ProjectManager.h"
#include "core/TaskManager.h"
#include "core/Executor.h"
#include "core/AgentManager.h"
#include "core/FileSystem.h"
#include "storage/MarkdownParser.h"
#include "storage/MarkdownWriter.h"
#include "ui/LayoutManager.h"
#include "ui/Theme.h"
#include <sstream>
#include <cstring>

void TaskDetailPanel::Render(ProjectManager& pm, TaskManager& tm,
    Executor& exec, AgentManager& agent_mgr, const LayoutManager& layout)
{
    if (!open_) return;

    auto rect = layout.GetPanelRect(PanelArea::Right);
    ImGui::SetNextWindowPos(rect.Min);
    ImGui::SetNextWindowSize(rect.GetSize());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("Task Detail", &open_,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (!current_task_ || !current_subtask_) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1),
            "Select a subtask from the board to view details.");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    auto* project = pm.GetActiveProject();
    if (!project) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1), "No active project.");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    auto& task = *current_task_;
    auto& sub = *current_subtask_;

    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1), "%s: %s", task.id.c_str(), task.title.c_str());
    ImGui::Separator();

    RenderSubtaskDetail(task, sub, exec, agent_mgr, *project);
    ImGui::Separator();

    RenderConversation(sub);
    ImGui::Separator();

    // Reply input
    ImGui::Text("Reply:");
    ImGui::InputTextMultiline("##reply", reply_input_, sizeof(reply_input_),
        ImVec2(-1, 60), ImGuiInputTextFlags_AllowTabInput);

    ImGui::BeginDisabled(std::string(reply_input_).empty());
    if (ImGui::Button("Send Reply", ImVec2(120, 0))) {
        std::string body(reply_input_);
        tm.AddReply(task, sub.id, "user", tm.Now(), body);

        auto task_path = project->TaskFilePath(task.id, task.title);
        auto content = FileSystem::ReadFile(task_path);
        if (!content.empty()) {
            auto updated = MarkdownWriter::AddConversationMsg(
                content, sub.id, "user", tm.Now(), body, false);
            FileSystem::WriteFile(task_path, updated);
        }

        tm.AppendAudit(project->AuditFilePath(), tm.Now(), "subtask_reply",
                       "user", "@" + sub.id + " " + body);

        reply_input_[0] = '\0';
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Refresh", ImVec2(80, 0))) {
        auto task_path = project->TaskFilePath(task.id, task.title);
        auto content = FileSystem::ReadFile(task_path);
        if (!content.empty()) {
            auto reloaded = MarkdownParser::ParseTask(content);
            task.subtasks = reloaded.subtasks;
            task.status = reloaded.status;
            task.description = reloaded.description;
            for (auto& s : task.subtasks) {
                if (s.id == sub.id) {
                    current_subtask_ = &s;
                    break;
                }
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void TaskDetailPanel::RenderSubtaskDetail(const Task& task, Subtask& sub,
    Executor& exec, AgentManager& agent_mgr, const Project& project)
{
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.9f, 1), "%s: %s", sub.id.c_str(), sub.title.c_str());
    ImGui::SameLine();

    auto status_color = Theme::ColorForStatus(task.StatusString(sub.status));
    ImGui::TextColored(ImVec4(
        ((status_color >> 24) & 0xFF) / 255.0f,
        ((status_color >> 16) & 0xFF) / 255.0f,
        ((status_color >> 8) & 0xFF) / 255.0f, 1),
        " [%s]", task.StatusString(sub.status).c_str());

    if (!sub.assignee.empty()) {
        ImGui::Text("Assignee: %s", sub.assignee.c_str());
    }

    std::string risk_str;
    switch (sub.risk) {
        case RiskLevel::Low: risk_str = "low"; break;
        case RiskLevel::Medium: risk_str = "medium"; break;
        case RiskLevel::High: risk_str = "high"; break;
    }
    ImGui::Text("Risk: %s", risk_str.c_str());

    if (!sub.result.empty()) {
        ImGui::Text("Result: %s", sub.result.c_str());
    }

    if (!sub.description.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", sub.description.c_str());
    }

    ImGui::Spacing();

    if (sub.status == SubtaskStatus::Pending) {
        if (agent_buf_[0] == '\0') {
            strncpy(agent_buf_, sub.assignee.empty() ? "opencode" : sub.assignee.c_str(), sizeof(agent_buf_) - 1);
        }

        auto& agents = agent_mgr.GetGlobalAgents();
        if (!agents.empty()) {
            ImGui::Text("Agent:");
            ImGui::SameLine();
            ImGui::PushItemWidth(160);
            if (ImGui::BeginCombo("##agent_selector", agent_buf_)) {
                for (const auto& a : agents) {
                    bool is_selected = (strcmp(agent_buf_, a.id.c_str()) == 0);
                    if (ImGui::Selectable(a.id.c_str(), is_selected)) {
                        strncpy(agent_buf_, a.id.c_str(), sizeof(agent_buf_) - 1);
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            if (ImGui::Button("Run", ImVec2(80, 0))) {
                auto* agent = agent_mgr.FindAgent(std::string(agent_buf_));
                if (agent && agent->enabled) {
                    auto task_path = project.TaskFilePath(task.id, task.title);
                    auto content = FileSystem::ReadFile(task_path);
                    if (!content.empty()) {
                        auto updated = MarkdownWriter::UpdateSubtaskStatus(
                            content, sub.id, "in_progress", "");
                        FileSystem::WriteFile(task_path, updated);
                    }

                    exec.Execute(task, sub.id, *agent, project);
                }
            }
        }
    }

    if (sub.status == SubtaskStatus::InProgress) {
        if (ImGui::Button("Complete", ImVec2(80, 0))) {
            auto task_path = project.TaskFilePath(task.id, task.title);
            auto content = FileSystem::ReadFile(task_path);
            if (!content.empty()) {
                auto updated = MarkdownWriter::UpdateSubtaskStatus(
                    content, sub.id, "completed", "manual_complete");
                FileSystem::WriteFile(task_path, updated);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Fail", ImVec2(80, 0))) {
            auto task_path = project.TaskFilePath(task.id, task.title);
            auto content = FileSystem::ReadFile(task_path);
            if (!content.empty()) {
                auto updated = MarkdownWriter::UpdateSubtaskStatus(
                    content, sub.id, "failed", "manual_fail");
                FileSystem::WriteFile(task_path, updated);
            }
        }
        ImGui::SameLine();

        bool has_running_exec = false;
        std::string running_exec_id;
        for (const auto& e : exec.GetExecutions()) {
            if (e.subtask_id == sub.id && e.task_id == task.id &&
                e.status == Executor::Execution::Running) {
                has_running_exec = true;
                running_exec_id = e.id;
                break;
            }
        }
        if (has_running_exec) {
            if (ImGui::Button("Cancel Exec", ImVec2(100, 0))) {
                exec.Cancel(running_exec_id);
            }
        }
    }
}

void TaskDetailPanel::RenderConversation(const Subtask& sub) {
    ImGui::Text("Conversation");
    ImGui::Separator();

    if (sub.conversation.empty()) {
        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.5f, 1), "No messages yet.");
        return;
    }

    for (const auto& msg : sub.conversation) {
        if (msg.is_exec_log) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.4f, 1));
            ImGui::Text("Execution Log:");
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1));
            ImGui::TextWrapped("%s", msg.body.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
            continue;
        }

        if (!msg.body.empty()) {
            bool is_agent = (msg.sender != "user");
            ImVec4 name_color = is_agent ? ImVec4(0.4f, 0.7f, 1.0f, 1) : ImVec4(0.8f, 0.8f, 0.3f, 1);

            ImGui::TextColored(name_color, "%s", msg.sender.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("  (%s)", msg.timestamp.c_str());

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.9f, 1));
            ImGui::TextWrapped("%s", msg.body.c_str());
            ImGui::PopStyleColor();

            auto at_pos = msg.body.find('@');
            if (at_pos != std::string::npos) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 0.7f), " [@detected]");
            }

            ImGui::Spacing();
        }
    }
}
