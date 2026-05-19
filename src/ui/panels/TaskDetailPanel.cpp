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
#include <algorithm>
#include <iostream>

void TaskDetailPanel::Render(ProjectManager& pm, TaskManager& tm,
    Executor& exec, AgentManager& agent_mgr, const LayoutManager& layout)
{
    if (!open_) return;

    ImGui::Begin("Task Detail", &open_,
        ImGuiWindowFlags_NoCollapse);

    if (!HasSelection()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1),
            "Select a subtask from the board to view details.");
        ImGui::End();
        return;
    }

    auto* project = pm.GetActiveProject();
    if (!project) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1), "No active project.");
        ImGui::End();
        return;
    }

    auto& task = current_task_;
    auto& sub = task.subtasks[current_subtask_idx_];

    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1), "%s: %s", task.id.c_str(), task.title.c_str());

    if (task.subtasks.size() <= 1 && sub.status == SubtaskStatus::Pending) {
        ImGui::SameLine();
        if (ImGui::SmallButton("AI Decompose")) {
            show_decompose_popup_ = true;
        }
        if (show_decompose_popup_) {
            ImGui::OpenPopup("Decompose Task");
            show_decompose_popup_ = false;
        }
    }

    if (ImGui::BeginPopupModal("Decompose Task", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Decompose '%s' into subtasks using AI or local rules?", task.title.c_str());
        ImGui::Separator();
        if (ImGui::Button("Use AI (first available agent)", ImVec2(200, 0))) {
            TriggerDecompose(task, *project, exec, agent_mgr, tm);
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Local (split by sentence)", ImVec2(200, 0))) {
            auto new_subs = DecomposeTaskLocal(task.description);
            for (auto& ns : new_subs) {
                task.subtasks.push_back(ns);
            }
            auto task_path = project->TaskFilePath(task.id, task.title);
            auto content = FileSystem::ReadFile(task_path);
            if (!content.empty()) {
                auto updated = MarkdownWriter::UpdateTaskSubtasks(content, task.subtasks);
                FileSystem::WriteFile(task_path, updated);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::Separator();
        if (ImGui::Button("Cancel", ImVec2(200, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    RenderSubtaskDetail(task, sub, exec, agent_mgr, *project);
    ImGui::Separator();

    RenderConversation(sub);
    ImGui::Separator();

    // Reply input with @autocomplete
    ImGui::Text("Reply:");
    ImGui::InputTextMultiline("##reply", reply_input_, sizeof(reply_input_),
        ImVec2(-1, 60), ImGuiInputTextFlags_AllowTabInput);

    // Detect @ for autocomplete
    std::string input(reply_input_);
    auto at_pos = input.rfind('@');
    if (at_pos != std::string::npos) {
        std::string partial = input.substr(at_pos + 1);
        ShowAgentAutocomplete(partial, agent_mgr);
    } else {
        show_autocomplete_ = false;
    }

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

        // Trigger agent execution if @agent detected
        HandleAgentTrigger(body, sub, task, exec, agent_mgr, *project);

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
            for (int i = 0; i < (int)task.subtasks.size(); i++) {
                if (task.subtasks[i].id == sub.id) {
                    current_subtask_idx_ = i;
                    break;
                }
            }
        }
    }

    ImGui::End();
}

void TaskDetailPanel::ShowAgentAutocomplete(const std::string& partial,
    AgentManager& agent_mgr)
{
    auto& agents = agent_mgr.GetGlobalAgents();

    std::vector<Agent*> matches;
    for (auto& a : agents) {
        if (!a.enabled) continue;
        if (partial.empty() ||
            a.id.find(partial) == 0 ||
            a.name.find(partial) == 0) {
            matches.push_back(&a);
        }
    }

    if (matches.empty()) {
        show_autocomplete_ = false;
        return;
    }

    show_autocomplete_ = true;
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(20, 20, 28, 240));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::BeginChild("##agent_complete",
        ImVec2(200, 28.0f * (int)matches.size() + 4),
        true, ImGuiWindowFlags_Tooltip);

    for (int i = 0; i < (int)matches.size(); i++) {
        auto* a = matches[i];
        bool selected = false;
        std::string label = a->id + " (" + a->name + ")";
        if (ImGui::Selectable(label.c_str(), &selected)) {
            auto at_pos = std::string(reply_input_).rfind('@');
            std::string new_input = std::string(reply_input_).substr(0, at_pos + 1)
                + a->id + " ";
            strncpy(reply_input_, new_input.c_str(), sizeof(reply_input_) - 1);
            show_autocomplete_ = false;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void TaskDetailPanel::HandleAgentTrigger(const std::string& body,
    Subtask& sub, const Task& task,
    Executor& exec, AgentManager& agent_mgr, const Project& project)
{
    auto at_pos = body.find('@');
    if (at_pos == std::string::npos) return;

    auto after_at = body.substr(at_pos + 1);
    auto space = after_at.find(' ');
    std::string agent_id = (space == std::string::npos) ? after_at : after_at.substr(0, space);

    auto* agent = agent_mgr.FindAgent(agent_id);
    if (!agent || !agent->enabled) return;

    auto task_path = project.TaskFilePath(task.id, task.title);
    auto content = FileSystem::ReadFile(task_path);
    if (!content.empty()) {
        auto updated = MarkdownWriter::UpdateSubtaskStatus(
            content, sub.id, "in_progress", "");
        FileSystem::WriteFile(task_path, updated);
    }

    exec.Execute(task, sub.id, *agent, project);
}

void TaskDetailPanel::RenderSubtaskDetail(Task& task, Subtask& sub,
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
                    if (!a.enabled) continue;
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
                std::string agent_id(agent_buf_);
                std::cerr << "[TaskDetailPanel] Run clicked: agent_buf_='" << agent_id
                          << "', task.id=" << task.id << ", sub.id=" << sub.id << "\n";
                auto* agent = agent_mgr.FindAgent(agent_id);
                if (!agent) {
                    std::cerr << "[TaskDetailPanel] Agent '" << agent_id << "' not found (available:";
                    for (const auto& a : agent_mgr.GetGlobalAgents())
                        std::cerr << " " << a.id;
                    std::cerr << ")\n";
                } else if (!agent->enabled) {
                    std::cerr << "[TaskDetailPanel] Agent '" << agent_id << "' is disabled\n";
                } else {
                    auto task_path = project.TaskFilePath(task.id, task.title);
                    std::cerr << "[TaskDetailPanel] task_path=" << task_path << "\n";
                    auto content = FileSystem::ReadFile(task_path);
                    if (!content.empty()) {
                        auto updated = MarkdownWriter::UpdateSubtaskStatus(
                            content, sub.id, "in_progress", "");
                        FileSystem::WriteFile(task_path, updated);
                        std::cerr << "[TaskDetailPanel] Status updated to in_progress\n";
                    } else {
                        std::cerr << "[TaskDetailPanel] Task file is empty or not found\n";
                    }

                    exec.Execute(task, sub.id, *agent, project);
                    std::cerr << "[TaskDetailPanel] exec.Execute returned\n";
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

    // Review status: approval buttons
    if (sub.status == SubtaskStatus::Review) {
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1), "\u26A0 Pending Approval");
        ImGui::SameLine();
        ImGui::TextDisabled("(risk: %s)", risk_str.c_str());

        if (ImGui::Button("Approve", ImVec2(100, 0))) {
            auto task_path = project.TaskFilePath(task.id, task.title);
            auto content = FileSystem::ReadFile(task_path);
            if (!content.empty()) {
                auto updated = MarkdownWriter::UpdateSubtaskStatus(
                    content, sub.id, "in_progress", "");
                FileSystem::WriteFile(task_path, updated);
                exec.Execute(task, sub.id, *agent_mgr.FindAgent(agent_buf_), project);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reject", ImVec2(100, 0))) {
            auto task_path = project.TaskFilePath(task.id, task.title);
            auto content = FileSystem::ReadFile(task_path);
            if (!content.empty()) {
                auto updated = MarkdownWriter::UpdateSubtaskStatus(
                    content, sub.id, "cancelled", "rejected by user");
                FileSystem::WriteFile(task_path, updated);
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

    int msg_idx = 0;
    for (const auto& msg : sub.conversation) {
        ImGui::PushID(msg_idx++);
        if (msg.is_exec_log) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.4f, 1));
            ImGui::Text("Execution Log:");
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1));
            ImGui::TextWrapped("%s", msg.body.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::PopID();
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
        ImGui::PopID();
    }
}

std::vector<Subtask> TaskDetailPanel::DecomposeTaskLocal(const std::string& description) {
    std::vector<Subtask> subs;
    if (description.empty()) {
        Subtask s;
        s.id = "ST-001";
        s.title = "Implement task";
        s.description = "";
        s.risk = RiskLevel::Medium;
        subs.push_back(s);
        return subs;
    }

    std::istringstream stream(description);
    std::string line;
    int count = 1;
    while (std::getline(stream, line)) {
        auto trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
        trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
        if (trimmed.size() < 20) continue;

        Subtask s;
        char id_buf[16];
        snprintf(id_buf, sizeof(id_buf), "ST-%03d", count);
        s.id = id_buf;
        s.title = trimmed.size() > 40 ? trimmed.substr(0, 40) + "..." : trimmed;
        s.description = trimmed;
        s.risk = RiskLevel::Medium;
        subs.push_back(s);
        count++;
    }

    if (subs.empty()) {
        Subtask s;
        s.id = "ST-001";
        s.title = "Implement " + (description.size() > 40 ? description.substr(0, 40) + "..." : description);
        s.description = description;
        s.risk = RiskLevel::Medium;
        subs.push_back(s);
    }

    return subs;
}

void TaskDetailPanel::TriggerDecompose(Task& task, const Project& project,
    Executor& exec, AgentManager& agent_mgr, TaskManager& tm)
{
    auto* agent = agent_mgr.FindAgent("opencode");
    if (!agent || !agent->enabled) {
        agent = nullptr;
        for (auto& a : agent_mgr.GetGlobalAgents()) {
            if (a.enabled) {
                agent = &a;
                break;
            }
        }
    }

    std::string decompose_prompt =
        "\u8BF7\u5C06\u4EE5\u4E0B\u9700\u6C42\u62C6\u89E3\u4E3A\u591A\u4E2A\u5B50\u4EFB\u52A1\uFF08Subtask\uFF09\uFF0C\u6BCF\u4E2A\u5B50\u4EFB\u52A1\u5305\u542B\u6807\u9898\u3001\u63CF\u8FF0\u3001\u98CE\u9669\u7EA7\u522B\uFF08low/medium/high\uFF09\u3002"
        "\u8BF7\u4EE5 Markdown \u5217\u8868\u683C\u5F0F\u8FD4\u56DE\uFF1A\n\n"
        "- ST-001: <title>\n"
        "  - description: <desc>\n"
        "  - risk: <level>\n\n"
        "\u9700\u6C42\u63CF\u8FF0\uFF1A\n" + task.description;

    if (agent && agent->enabled) {
        std::string fake_id = "decompose-" + task.id;
        Subtask decompose_sub;
        decompose_sub.id = fake_id;
        decompose_sub.title = "AI Decompose: " + task.title;
        decompose_sub.description = decompose_prompt;
        decompose_sub.risk = RiskLevel::Low;
        task.subtasks.insert(task.subtasks.begin(), decompose_sub);

        exec.Execute(task, fake_id, *agent, project);
    } else {
        auto new_subs = DecomposeTaskLocal(task.description);
        for (auto& ns : new_subs) {
            task.subtasks.push_back(ns);
        }
        auto task_path = project.TaskFilePath(task.id, task.title);
        auto content = FileSystem::ReadFile(task_path);
        if (!content.empty()) {
            auto updated = MarkdownWriter::UpdateTaskSubtasks(content, task.subtasks);
            FileSystem::WriteFile(task_path, updated);
        }
    }
}