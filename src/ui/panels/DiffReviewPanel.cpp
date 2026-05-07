#include "DiffReviewPanel.h"
#include "ui/LayoutManager.h"
#include <sstream>

void DiffReviewPanel::Render(const LayoutManager& layout) {
    if (!open_) return;

    ImGui::Begin("Diff Review", &open_,
        ImGuiWindowFlags_NoCollapse);

    if (diff_content_.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1),
            "No diff to review. Run a subtask to see changes.");
        ImGui::End();
        return;
    }

    // Show diff content
    ImVec2 content_size = ImGui::GetContentRegionAvail();
    content_size.y -= 36;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(14, 14, 18, 255));
    ImGui::BeginChild("##diff_content", content_size, true, ImGuiWindowFlags_HorizontalScrollbar);

    // Simple line-by-line diff rendering
    std::istringstream stream(diff_content_);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.size() >= 1) {
            char first = line[0];
            if (first == '+') {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(80, 200, 120, 255));
                ImGui::TextWrapped("%s", line.c_str());
                ImGui::PopStyleColor();
            } else if (first == '-') {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 60, 60, 255));
                ImGui::TextWrapped("%s", line.c_str());
                ImGui::PopStyleColor();
            } else if (line.size() >= 3 && line[0] == '@' && line[1] == '@') {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 150, 220, 255));
                ImGui::TextWrapped("%s", line.c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::TextWrapped("%s", line.c_str());
            }
        }
    }

    if (auto_scroll_) {
        ImGui::SetScrollHereY(0);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Action buttons
    ImGui::Checkbox("Auto-scroll", &auto_scroll_);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200);
    if (ImGui::Button("Approve All", ImVec2(100, 0))) {
        diff_content_.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reject All", ImVec2(100, 0))) {
        diff_content_.clear();
    }

    ImGui::End();
}