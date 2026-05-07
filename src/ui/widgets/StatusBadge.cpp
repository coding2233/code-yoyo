#include "StatusBadge.h"
#include "ui/Theme.h"

namespace StatusBadge {

void Render(const std::string& status, const std::string& label) {
    auto color = Theme::ColorForStatus(status);
    auto icon = Theme::StatusIcon(status);

    std::string display = label.empty()
        ? std::string(icon) + " " + status
        : std::string(icon) + " " + label;

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(
        ((color >> 24) & 0xFF) / 255.0f,
        ((color >> 16) & 0xFF) / 255.0f,
        ((color >> 8) & 0xFF) / 255.0f, 1));
    ImGui::TextUnformatted(display.c_str());
    ImGui::PopStyleColor();
}

void RenderRisk(const std::string& risk) {
    auto color = Theme::ColorForRisk(risk);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(
        ((color >> 24) & 0xFF) / 255.0f,
        ((color >> 16) & 0xFF) / 255.0f,
        ((color >> 8) & 0xFF) / 255.0f, 1));
    ImGui::Text("◈ %s", risk.c_str());
    ImGui::PopStyleColor();
}

void RenderExecutionStatus(int status_code) {
    if (status_code == 0) {
        auto color = Theme::ColorForStatus("completed");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.8f, 0.4f, 1));
        ImGui::Text("✅ Exit: %d", status_code);
        ImGui::PopStyleColor();
    } else if (status_code > 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1));
        ImGui::Text("❌ Exit: %d", status_code);
        ImGui::PopStyleColor();
    }
}

} // namespace StatusBadge