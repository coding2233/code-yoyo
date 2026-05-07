#include "LayoutManager.h"

static float topbar_height_ = 36.0f;
static float menubar_height_ = 0.0f;

void LayoutManager::Init() {
    initial_layout_done_ = false;
}

void LayoutManager::Shutdown() {
}

void LayoutManager::BeginFrame() {
    auto viewport = ImGui::GetMainViewport();
    float menu_offset = menubar_height_ + topbar_height_;

    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + menu_offset));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - menu_offset));
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("MainDockSpace", nullptr, flags);
    ImGui::PopStyleVar(2);

    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    if (!initial_layout_done_ || reset_layout_) {
        SetupInitialLayout();
        initial_layout_done_ = true;
        reset_layout_ = false;
    }
}

void LayoutManager::EndFrame() {
    ImGui::End();
}

void LayoutManager::SetupInitialLayout() {
    auto viewport = ImGui::GetMainViewport();
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

    ImGuiID dock_left, dock_right, dock_bottom, dock_center;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.15f, &dock_left, &dock_center);
    ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Right, 0.25f, &dock_right, &dock_center);
    ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Down, 0.25f, &dock_bottom, &dock_center);

    ImGui::DockBuilderDockWindow("Projects", dock_left);
    ImGui::DockBuilderDockWindow("Task Board", dock_center);
    ImGui::DockBuilderDockWindow("Task Tree", dock_center);
    ImGui::DockBuilderDockWindow("Scheduled Tasks", dock_center);
    ImGui::DockBuilderDockWindow("Diff Review", dock_center);
    ImGui::DockBuilderDockWindow("Settings", dock_center);
    ImGui::DockBuilderDockWindow("Task Detail", dock_right);
    ImGui::DockBuilderDockWindow("Agent Console", dock_bottom);

    ImGui::DockBuilderFinish(dockspace_id);
}
