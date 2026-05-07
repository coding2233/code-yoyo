#include "Theme.h"
#include "core/FileSystem.h"

namespace Theme {

static ImFont* g_default_font = nullptr;
static ImFont* g_icon_font = nullptr;

static std::string FindFontsDir() {
    const char* candidates[] = {
        "fonts/",
        "../fonts/",
        "../../fonts/",
        "../../../fonts/",
    };
    for (auto* dir : candidates) {
        std::string test = std::string(dir) + "SourceCodePro-Medium.ttf";
        if (FileSystem::FileExists(test)) {
            return dir;
        }
    }
    return "fonts/";
}

void LoadFonts() {
    auto& io = ImGui::GetIO();
    io.Fonts->Clear();

    std::string fonts_dir = FindFontsDir();

    ImFontConfig main_cfg;
    main_cfg.OversampleH = 1;
    main_cfg.OversampleV = 1;
    main_cfg.PixelSnapH = true;
    std::string scp_path = fonts_dir + "SourceCodePro-Medium.ttf";
    g_default_font = io.Fonts->AddFontFromFileTTF(scp_path.c_str(), 16.0f, &main_cfg);

    ImFontConfig merge_cfg;
    merge_cfg.MergeMode = true;
    merge_cfg.OversampleH = 1;
    merge_cfg.OversampleV = 1;
    std::string wqy_path = fonts_dir + "wqy-microhei.ttc";
    io.Fonts->AddFontFromFileTTF(wqy_path.c_str(), 16.0f, &merge_cfg);

    ImFontConfig icon_cfg;
    icon_cfg.MergeMode = true;
    icon_cfg.OversampleH = 1;
    icon_cfg.OversampleV = 1;
    icon_cfg.GlyphMinAdvanceX = 16.0f;
    icon_cfg.GlyphOffset = ImVec2(0, 2);
    std::string mat_path = fonts_dir + "MaterialIcons-Regular.ttf";
    g_icon_font = io.Fonts->AddFontFromFileTTF(mat_path.c_str(), 16.0f, &icon_cfg);

    io.Fonts->Build();
}

ImFont* GetDefaultFont() {
    return g_default_font;
}

ImFont* GetIconFont() {
    return g_icon_font;
}

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
    if (status == "pending")     return "\u25CB";
    if (status == "in_progress") return "\u25C9";
    if (status == "review")      return "\u25D0";
    if (status == "completed")   return "\u25CF";
    if (status == "failed")      return "\u2715";
    if (status == "cancelled")   return "\u2014";
    return "\u25CB";
}

}
