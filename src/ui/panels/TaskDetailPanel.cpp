#include "TaskDetailPanel.h"
#include "core/ProjectManager.h"
#include "core/TaskManager.h"
#include "core/FileSystem.h"
#include "storage/MarkdownParser.h"
#include "storage/MarkdownWriter.h"
#include "ui/Theme.h"
#include <sstream>

void TaskDetailPanel::Render(ProjectManager& pm, TaskManager& tm) {
    if (!open_) return;

    ImGui::Begin("Task Detail", &open_);

    if (!current_task_ || !current_subtask_) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1),
            "Select a subtask from the board to view details.");
        ImGui::End();
        return;
    }

    auto& task = *current_task_;
    auto& sub = *current_subtask_;

    // Task header
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1), "%s: %s", task.id.c_str(), task.title.c_str());
    ImGui::Separator();

    // Subtask detail
    RenderSubtaskDetail(task, sub);
    ImGui::Separator();

    // Conversation thread
    RenderConversation(sub);
    ImGui::Separator();

    // Reply input
    auto* project = pm.GetActiveProject();
    if (project) {
        ImGui::Text("Reply:");
        ImGui::InputTextMultiline("##reply", reply_input_, sizeof(reply_input_),
            ImVec2(-1, 60), ImGuiInputTextFlags_AllowTabInput);

        ImGui::BeginDisabled(std::string(reply_input_).empty());
        if (ImGui::Button("Send Reply", ImVec2(120, 0))) {
            std::string body(reply_input_);
            tm.AddReply(task, sub.id, "user", tm.Now(), body);

            // Update file
            auto task_path = project->TaskFilePath(task.id, task.title);
            auto content = FileSystem::ReadFile(task_path);
            auto updated = MarkdownWriter::AddConversationMsg(
                content, sub.id, "user", tm.Now(), body, false);
            FileSystem::WriteFile(task_path, updated);

            // Audit
            tm.AppendAudit(project->AuditFilePath(), tm.Now(), "subtask_reply",
                           "user", "@" + sub.id + " " + body);

            reply_input_[0] = '\0';
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Refresh", ImVec2(80, 0))) {
            // Reload task data from file
            auto task_path = project->TaskFilePath(task.id, task.title);
            auto content = FileSystem::ReadFile(task_path);
            if (!content.empty()) {
                auto reloaded = MarkdownParser::ParseTask(content);
                task.subtasks = reloaded.subtasks;
                task.status = reloaded.status;
                task.description = reloaded.description;
                // Re-find the subtask
                for (auto& s : task.subtasks) {
                    if (s.id == sub.id) {
                        current_subtask_ = &s;
                        break;
                    }
                }
            }
        }
    }

    ImGui::End();
}

void TaskDetailPanel::RenderSubtaskDetail(const Task& task, Subtask& sub) {
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.9f, 1), "%s: %s", sub.id.c_str(), sub.title.c_str());
    ImGui::SameLine();

    auto status_color = Theme::ColorForStatus(task.StatusString(sub.status));
    ImGui::TextColored(ImVec4(
        ((status_color >> 24) & 0xFF) / 255.0f,
        ((status_color >> 16) & 0xFF) / 255.0f,
        ((status_color >> 8) & 0xFF) / 255.0f, 1),
        " [%s]", task.StatusString(sub.status).c_str());

    // Metadata
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

    // Description
    if (!sub.description.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", sub.description.c_str());
    }

    // Action buttons
    ImGui::Spacing();
    if (sub.status == SubtaskStatus::Pending) {
        if (ImGui::Button("Start", ImVec2(80, 0))) {
            auto* project = nullptr; // We'll handle this differently
        }
        ImGui::SameLine();
    }
    if (sub.status == SubtaskStatus::InProgress) {
        if (ImGui::Button("Complete", ImVec2(80, 0))) {
            // TODO: implement
        }
        ImGui::SameLine();
        if (ImGui::Button("Fail", ImVec2(80, 0))) {
            // TODO: implement
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

            // Detect @mentions in the message
            auto at_pos = msg.body.find('@');
            if (at_pos != std::string::npos) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 0.7f), " [@detected]");
            }

            ImGui::Spacing();
        }
    }
}
