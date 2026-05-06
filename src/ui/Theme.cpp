#include "Theme.h"

namespace Theme {

void ApplyDark() {
    auto& style = ImGui::GetStyle();
    auto& colors = style.Colors;

    colors[ImGuiCol_WindowBg]          = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg]           = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_PopupBg]           = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_FrameBg]           = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]    = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgActive]     = ImVec4(0.26f, 0.26f, 0.28f, 1.00f);
    colors[ImGuiCol_TitleBg]           = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]     = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_MenuBarBg]         = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_Header]            = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_HeaderHovered]     = ImVec4(0.26f, 0.26f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderActive]      = ImVec4(0.30f, 0.30f, 0.34f, 1.00f);
    colors[ImGuiCol_Button]            = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonHovered]     = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
    colors[ImGuiCol_ButtonActive]      = ImVec4(0.34f, 0.34f, 0.38f, 1.00f);
    colors[ImGuiCol_Tab]               = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_TabHovered]        = ImVec4(0.24f, 0.24f, 0.28f, 1.00f);
    colors[ImGuiCol_TabActive]         = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_TabUnfocused]      = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]= ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
    colors[ImGuiCol_Text]              = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]      = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);
    colors[ImGuiCol_Border]            = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_Separator]         = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]       = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]     = ImVec4(0.24f, 0.24f, 0.28f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.34f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.36f, 0.36f, 0.40f, 1.00f);
    colors[ImGuiCol_DockingPreview]    = ImVec4(0.30f, 0.50f, 0.80f, 0.60f);
    colors[ImGuiCol_DockingEmptyBg]    = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);

    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 3.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 3.0f;

    style.WindowPadding     = ImVec2(8, 8);
    style.FramePadding      = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(6, 4);
    style.ItemInnerSpacing  = ImVec2(4, 4);
    style.IndentSpacing     = 16.0f;
    style.ScrollbarSize     = 14.0f;
    style.GrabMinSize       = 8.0f;
}

ImU32 ColorForStatus(const std::string& status) {
    if (status == "pending")    return IM_COL32(100, 130, 180, 255);
    if (status == "in_progress")return IM_COL32(60,  160, 220, 255);
    if (status == "review")     return IM_COL32(220, 180, 60,  255);
    if (status == "completed")  return IM_COL32(80,  200, 120, 255);
    if (status == "failed")     return IM_COL32(220, 60,  60,  255);
    if (status == "cancelled")  return IM_COL32(140, 140, 150, 255);
    return IM_COL32(160, 160, 170, 255);
}

ImU32 ColorForRisk(const std::string& risk) {
    if (risk == "low")    return IM_COL32(80,  200, 120, 255);
    if (risk == "medium") return IM_COL32(220, 180, 60,  255);
    if (risk == "high")   return IM_COL32(220, 60,  60,  255);
    return IM_COL32(160, 160, 170, 255);
}

const char* StatusIcon(const std::string& status) {
    if (status == "pending")     return "○";
    if (status == "in_progress") return "◉";
    if (status == "review")      return "◐";
    if (status == "completed")   return "●";
    if (status == "failed")      return "✕";
    if (status == "cancelled")   return "—";
    return "○";
}

}
