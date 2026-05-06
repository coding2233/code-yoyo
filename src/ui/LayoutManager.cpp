#include "LayoutManager.h"

void LayoutManager::Init() {
    initialized_ = true;
}

void LayoutManager::Shutdown() {
    initialized_ = false;
}

void LayoutManager::BeginDockspace() {
    auto viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("DockSpace", nullptr, flags);
    ImGui::PopStyleVar(3);

    dockspace_id_ = ImGui::GetID("MyDockspace");
    ImGui::DockSpace(dockspace_id_, ImVec2(0, 0), ImGuiDockNodeFlags_None);
}

void LayoutManager::EndDockspace() {
    ImGui::End();
}
