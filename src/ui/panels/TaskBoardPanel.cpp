#include "TaskBoardPanel.h"
#include "core/ProjectManager.h"
#include "core/TaskManager.h"
#include "core/FileSystem.h"
#include "storage/MarkdownParser.h"
#include "storage/MarkdownWriter.h"
#include "ui/LayoutManager.h"
#include "ui/Theme.h"
#include <map>

void TaskBoardPanel::Render(ProjectManager& pm, TaskManager& tm, const LayoutManager& layout) {
    if (!open_) return;

    auto rect = layout.GetPanelRect(PanelArea::Center);
    ImGui::SetNextWindowPos(rect.Min);
    ImGui::SetNextWindowSize(rect.GetSize());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("Task Board", &open_,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    auto* project = pm.GetActiveProject();
    if (!project) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1), "Select a project to get started.");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    if (ImGui::Button("+ New Task")) {
        show_new_task_popup_ = true;
    }
    ImGui::SameLine();
    ImGui::Text("  |  %s  [%s]", project->name.c_str(), project->repo.c_str());
    ImGui::Separator();

    auto task_files = FileSystem::ListFiles(project->tasks_path, ".md");
    std::vector<Task> tasks;
    for (const auto& f : task_files) {
        auto content = FileSystem::ReadFile(f);
        if (!content.empty()) {
            tasks.push_back(MarkdownParser::ParseTask(content));
        }
    }

    if (show_new_task_popup_) {
        ImGui::OpenPopup("New Task");
        show_new_task_popup_ = false;
    }
    if (ImGui::BeginPopupModal("New Task", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Task Title", new_task_title_, sizeof(new_task_title_));
        if (ImGui::Button("Create")) {
            std::string title(new_task_title_);
            if (!title.empty()) {
                std::string id = tm.GenerateTaskId(tasks);
                auto task = tm.CreateTask(project->tasks_path, title, "");
                task.id = id;
                task.status = "active";
                task.created = tm.Now();
                task.subtasks.push_back(tm.CreateSubtask("Initial task", ""));
                auto path = project->TaskFilePath(id, title);
                tm.UpdateTaskFile(path, task);
                tm.AppendAudit(project->AuditFilePath(), tm.Now(), "task_create", "user",
                               "Created " + id);
                new_task_title_[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    std::map<SubtaskStatus, std::vector<std::pair<Task*, Subtask*>>> grouped;
    for (auto& task : tasks) {
        for (auto& sub : task.subtasks) {
            grouped[sub.status].push_back({&task, &sub});
        }
    }

    const float avail = ImGui::GetContentRegionAvail().x;
    const float col_width = (avail - ImGui::GetStyle().ItemSpacing.x * 3) / 4.0f;

    struct ColumnDef {
        std::string title;
        std::string status_str;
        SubtaskStatus filter;
    };
    ColumnDef cols[] = {
        {"Pending",     "pending",     SubtaskStatus::Pending},
        {"In Progress", "in_progress", SubtaskStatus::InProgress},
        {"Review",      "review",      SubtaskStatus::Review},
        {"Completed",   "completed",   SubtaskStatus::Completed},
    };

    for (int ci = 0; ci < 4; ci++) {
        if (ci > 0) ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);

        ImGui::BeginGroup();
        ImGui::PushID(ci);

        auto header_color = Theme::ColorForStatus(cols[ci].status_str);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, header_color & 0x00FFFFFF | 0x18000000);
        ImGui::BeginChild(("hdr" + cols[ci].title).c_str(), ImVec2(col_width, 32), true);
        ImGui::TextColored(ImVec4(
            ((header_color >> 24) & 0xFF) / 255.0f,
            ((header_color >> 16) & 0xFF) / 255.0f,
            ((header_color >> 8) & 0xFF) / 255.0f, 1),
            "%s  %s", Theme::StatusIcon(cols[ci].status_str), cols[ci].title.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();

        float body_height = ImGui::GetContentRegionAvail().y;
        ImGui::BeginChild(("body" + cols[ci].title).c_str(), ImVec2(col_width, body_height), true);

        // Drop target for the column
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_SUBTASK")) {
                std::string dropped_id((const char*)payload->Data);
                auto task_files2 = FileSystem::ListFiles(project->tasks_path, ".md");
                for (const auto& f : task_files2) {
                    auto content = FileSystem::ReadFile(f);
                    if (content.empty()) continue;
                    auto updated = MarkdownWriter::UpdateSubtaskStatus(
                        content, dropped_id, cols[ci].status_str, "");
                    if (updated != content) {
                        FileSystem::WriteFile(f, updated);
                        break;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        auto& items = grouped[cols[ci].filter];
        for (auto& [task, sub] : items) {
            RenderCard(*sub);
        }

        ImGui::EndChild();
        ImGui::PopID();
        ImGui::EndGroup();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void TaskBoardPanel::RenderCard(const Subtask& sub) {
    ImGui::PushID(sub.id.c_str());

    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);

    ImU32 card_bg = IM_COL32(22, 22, 28, 220);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, card_bg);
    ImGui::BeginChild(("card" + sub.id).c_str(), ImVec2(0, 80), true);

    ImGui::Text("%s", sub.title.c_str());

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.7f, 1), "%s", sub.id.c_str());
    ImGui::SameLine();

    std::string risk_str;
    switch (sub.risk) {
        case RiskLevel::Low: risk_str = "low"; break;
        case RiskLevel::Medium: risk_str = "medium"; break;
        case RiskLevel::High: risk_str = "high"; break;
    }
    auto risk_color = Theme::ColorForRisk(risk_str);
    ImGui::TextColored(ImVec4(
        ((risk_color >> 24) & 0xFF) / 255.0f,
        ((risk_color >> 16) & 0xFF) / 255.0f,
        ((risk_color >> 8) & 0xFF) / 255.0f, 1),
        " %s", risk_str.c_str());

    if (!sub.assignee.empty()) {
        ImGui::TextDisabled("→ %s", sub.assignee.c_str());
    }

    if (ImGui::IsItemHovered() || ImGui::IsWindowHovered()) {
        if (ImGui::IsMouseClicked(0)) {
            selected_subtask_ = const_cast<Subtask*>(&sub);
        }
    }

    // Drag source
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("DND_SUBTASK", sub.id.c_str(), sub.id.size() + 1);
        ImGui::Text("Move %s", sub.id.c_str());
        ImGui::EndDragDropSource();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::EndGroup();

    ImGui::PopID();
}