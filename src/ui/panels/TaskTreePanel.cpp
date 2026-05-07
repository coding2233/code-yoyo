#include "TaskTreePanel.h"
#include "core/ProjectManager.h"
#include "core/FileSystem.h"
#include "storage/MarkdownParser.h"
#include "ui/LayoutManager.h"
#include "ui/Theme.h"

void TaskTreePanel::Render(ProjectManager& pm, const LayoutManager& layout) {
    if (!open_) return;

    ImGui::Begin("Task Tree", &open_,
        ImGuiWindowFlags_NoCollapse);

    auto* project = pm.GetActiveProject();
    if (!project) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1), "Select a project to get started.");
        ImGui::End();
        return;
    }

    ImGui::Text("%s  [%s]", project->name.c_str(), project->repo.c_str());
    ImGui::Separator();

    auto task_files = FileSystem::ListFiles(project->tasks_path, ".md");
    std::vector<Task> tasks;
    for (const auto& f : task_files) {
        auto content = FileSystem::ReadFile(f);
        if (!content.empty()) {
            tasks.push_back(MarkdownParser::ParseTask(content));
        }
    }

    int task_idx = 0;
    for (auto& task : tasks) {
        ImGui::PushID(task.id.c_str());

        bool is_task_selected = (selected_task_idx_ == task_idx);
        ImVec4 task_color = is_task_selected ? ImVec4(0.4f, 0.7f, 1.0f, 1) : ImVec4(0.9f, 0.9f, 0.95f, 1);
        ImGui::TextColored(task_color, "%s: %s", task.id.c_str(), task.title.c_str());

        if (ImGui::IsItemClicked()) {
            selected_task_idx_ = task_idx;
            selected_sub_idx_ = -1;
        }

        // Indented subtasks
        int sub_idx = 0;
        for (auto& sub : task.subtasks) {
            ImGui::Indent(16);

            bool is_sub_selected = (selected_task_idx_ == task_idx && selected_sub_idx_ == sub_idx);
            auto status_color = Theme::ColorForStatus(task.StatusString(sub.status));
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.8f, 1), "  %s", sub.id.c_str());
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(
                ((status_color >> 24) & 0xFF) / 255.0f,
                ((status_color >> 16) & 0xFF) / 255.0f,
                ((status_color >> 8) & 0xFF) / 255.0f, 1),
                "[%s]", task.StatusString(sub.status).c_str());
            ImGui::SameLine();

            if (is_sub_selected) {
                ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1), " %s", sub.title.c_str());
            } else {
                ImGui::Text("%s", sub.title.c_str());
            }

            if (ImGui::IsItemClicked()) {
                selected_task_idx_ = task_idx;
                selected_sub_idx_ = sub_idx;
            }

            ImGui::Unindent(16);
            sub_idx++;
        }

        ImGui::Separator();
        ImGui::PopID();
        task_idx++;
    }

    ImGui::End();
}