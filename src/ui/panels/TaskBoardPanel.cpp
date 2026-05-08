#include "TaskBoardPanel.h"
#include "core/ProjectManager.h"
#include "core/TaskManager.h"
#include "core/FileSystem.h"
#include "storage/MarkdownParser.h"
#include "storage/MarkdownWriter.h"
#include "ui/LayoutManager.h"
#include "ui/Theme.h"
#include <map>

static bool UpdateSubtaskInFile(const std::string& tasks_path, const std::string& task_id,
                                 const std::string& sub_id, const std::string& new_status) {
    auto task_files = FileSystem::ListFiles(tasks_path, ".md");
    for (const auto& f : task_files) {
        auto content = FileSystem::ReadFile(f);
        if (content.empty()) continue;
        auto parsed = MarkdownParser::ParseTask(content);
        if (parsed.id != task_id) continue;
        auto updated = MarkdownWriter::UpdateSubtaskStatus(content, sub_id, new_status, "");
        if (updated != content) {
            FileSystem::WriteFile(f, updated);
            return true;
        }
        break;
    }
    return false;
}

void TaskBoardPanel::Render(ProjectManager& pm, TaskManager& tm, const LayoutManager& layout) {
    if (!open_) return;

    ImGui::Begin("Task Board", &open_, ImGuiWindowFlags_NoCollapse);

    auto* project = pm.GetActiveProject();
    if (!project) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1), "Select a project to get started.");
        ImGui::End();
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

    struct ColumnDef {
        std::string title;
        std::string status;
    };
    ColumnDef cols[] = {
        {"Planning",  "planning"},
        {"Active",    "active"},
        {"Completed", "completed"},
        {"Cancelled", "cancelled"},
    };

    std::map<std::string, std::vector<Task*>> grouped;
    for (auto& task : tasks) {
        grouped[task.status].push_back(&task);
    }

    ImVec2 content_avail = ImGui::GetContentRegionAvail();
    const float col_width = (content_avail.x - ImGui::GetStyle().ItemSpacing.x * 3) / 4.0f;
    const float body_height = content_avail.y - 40;

    for (int ci = 0; ci < 4; ci++) {
        if (ci > 0) ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);

        ImGui::BeginGroup();
        ImGui::PushID(ci);

        auto header_color = Theme::ColorForStatus(cols[ci].status);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, (header_color & 0x00FFFFFF) | 0x18000000);
        ImGui::BeginChild(("hdr" + cols[ci].title).c_str(), ImVec2(col_width, 32), true);
        ImGui::TextColored(ImVec4(
            ((header_color >> 24) & 0xFF) / 255.0f,
            ((header_color >> 16) & 0xFF) / 255.0f,
            ((header_color >> 8) & 0xFF) / 255.0f, 1),
            "%s  %s", Theme::StatusIcon(cols[ci].status), cols[ci].title.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::BeginChild(("body" + cols[ci].title).c_str(), ImVec2(col_width, body_height), true);

        auto& column_tasks = grouped[cols[ci].status];
        for (auto* task : column_tasks) {

            ImVec2 card_size = ImVec2(col_width - ImGui::GetStyle().WindowPadding.x * 2, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
            ImU32 card_bg = IM_COL32(22, 22, 28, 230);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, card_bg);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(50, 50, 60, 200));
            ImGui::BeginChild(("tcard" + task->id).c_str(), card_size, true);

            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1), "%s: %s",
                task->id.c_str(), task->title.c_str());

            int total = (int)task->subtasks.size();
            int done = 0;
            for (auto& sub : task->subtasks) {
                if (sub.status == SubtaskStatus::Completed) done++;
            }
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1), "%d/%d subtasks", done, total);

            ImGui::Spacing();
            ImGui::Separator();
            for (int si = 0; si < (int)task->subtasks.size(); si++) {
                auto& sub = task->subtasks[si];
                std::string uid = task->id + "::" + sub.id;
                ImGui::PushID(uid.c_str());

                bool is_selected = (selected_task_id_ == task->id && selected_sub_id_ == sub.id);

                auto st = task->StatusString(sub.status);
                auto st_color = Theme::ColorForStatus(st);

                ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(st_color));
                ImGui::Text("%s", Theme::StatusIcon(st));
                ImGui::PopStyleColor();

                ImGui::SameLine();
                std::string selectable_label = sub.title + "##" + uid;
                if (ImGui::Selectable(selectable_label.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    selected_task_id_ = task->id;
                    selected_sub_id_ = sub.id;
                }

                std::string popup_id = "ctx_" + uid;
                if (ImGui::BeginPopupContextItem(popup_id.c_str())) {
                    if (ImGui::MenuItem("Set Pending"))
                        UpdateSubtaskInFile(project->tasks_path, task->id, sub.id, "pending");
                    if (ImGui::MenuItem("Set In Progress"))
                        UpdateSubtaskInFile(project->tasks_path, task->id, sub.id, "in_progress");
                    if (ImGui::MenuItem("Set Completed"))
                        UpdateSubtaskInFile(project->tasks_path, task->id, sub.id, "completed");
                    if (ImGui::MenuItem("Set Failed"))
                        UpdateSubtaskInFile(project->tasks_path, task->id, sub.id, "failed");
                    if (ImGui::MenuItem("Set Cancelled"))
                        UpdateSubtaskInFile(project->tasks_path, task->id, sub.id, "cancelled");
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }

            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();

            ImGui::Spacing();
        }

        ImGui::EndChild();
        ImGui::PopID();
        ImGui::EndGroup();
    }

    ImGui::End();
}
