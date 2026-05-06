#include "ProjectPanel.h"
#include "core/ProjectManager.h"
#include "ui/LayoutManager.h"
#include "ui/Theme.h"
#include <imgui.h>

void ProjectPanel::Render(ProjectManager& pm, const LayoutManager& layout) {
    if (!open_) return;

    auto rect = layout.GetPanelRect(PanelArea::Left);
    ImGui::SetNextWindowPos(rect.Min);
    ImGui::SetNextWindowSize(rect.GetSize());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("Projects", &open_,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (ImGui::Button("+ New Project", ImVec2(-1, 0))) {
        show_new_popup_ = true;
    }

    ImGui::Separator();

    auto& projects = pm.GetProjects();
    for (auto& p : projects) {
        bool is_active = (pm.GetActiveProject() && pm.GetActiveProject()->name == p.name);

        ImGui::PushID(p.name.c_str());

        ImVec4 text_color = is_active ? ImVec4(0.4f, 0.7f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);
        ImGui::TextColored(text_color, "%s", p.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("  %s", p.status.c_str());

        if (ImGui::IsItemClicked() && !is_active) {
            pm.SetActiveProject(p.name);
        }

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Set Active")) {
                pm.SetActiveProject(p.name);
            }
            if (ImGui::MenuItem("Remove")) {
                pm.RemoveProject(p.name);
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    if (show_new_popup_) {
        ImGui::OpenPopup("New Project");
        show_new_popup_ = false;
    }

    RenderNewProjectPopup(pm);

    ImGui::End();
    ImGui::PopStyleVar();
}

void ProjectPanel::RenderNewProjectPopup(ProjectManager& pm) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create a new project");
        ImGui::Separator();

        ImGui::InputText("Project Name", new_project_name_, sizeof(new_project_name_));
        ImGui::InputText("Repo Path", new_project_repo_, sizeof(new_project_repo_));

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            std::string name(new_project_name_);
            std::string repo(new_project_repo_);
            if (!name.empty() && !repo.empty()) {
                pm.CreateProject(name, repo);
                new_project_name_[0] = '\0';
                new_project_repo_[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
