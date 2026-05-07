#include "SchedulePanel.h"
#include "core/ProjectManager.h"
#include "core/TaskManager.h"
#include "core/FileSystem.h"
#include "storage/MarkdownParser.h"
#include "ui/LayoutManager.h"
#include "ui/Theme.h"

void SchedulePanel::Render(ProjectManager& pm, TaskManager& tm, const LayoutManager& layout) {
    if (!open_) return;

    ImGui::Begin("Scheduled Tasks", &open_,
        ImGuiWindowFlags_NoCollapse);

    auto* project = pm.GetActiveProject();
    if (!project) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1), "No active project.");
        ImGui::End();
        return;
    }

    auto task_files = FileSystem::ListFiles(project->tasks_path, ".md");
    bool has_scheduled = false;

    for (const auto& f : task_files) {
        auto content = FileSystem::ReadFile(f);
        if (content.empty()) continue;
        auto task = MarkdownParser::ParseTask(content);

        if (task.schedule.empty()) continue;
        has_scheduled = true;

        ImGui::PushID(task.id.c_str());

        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1), "%s: %s", task.id.c_str(), task.title.c_str());

        ImGui::Indent(16);
        ImGui::Text("Schedule: %s", task.schedule.c_str());
        if (!task.timezone.empty()) {
            ImGui::Text("Timezone: %s", task.timezone.c_str());
        }
        if (!task.last_run.empty()) {
            ImGui::Text("Last Run: %s", task.last_run.c_str());
        }
        if (!task.next_run.empty()) {
            ImGui::Text("Next Run: %s", task.next_run.c_str());
        }
        if (!task.assignee.empty()) {
            ImGui::Text("Assignee: %s", task.assignee.c_str());
        }

        // Show subtask status
        int total = (int)task.subtasks.size();
        int completed = 0;
        for (const auto& sub : task.subtasks) {
            if (sub.status == SubtaskStatus::Completed) completed++;
        }
        ImGui::Text("Progress: %d/%d subtasks", completed, total);

        ImGui::Unindent(16);
        ImGui::Separator();
        ImGui::PopID();
    }

    if (!has_scheduled) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1),
            "No scheduled tasks. Add a 'schedule' field to a task.");
    }

    ImGui::End();
}