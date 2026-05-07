#include "ConversationThread.h"

namespace ConversationThread {

void Render(const std::vector<ConversationMsg>& messages) {
    if (messages.empty()) {
        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.5f, 1), "No messages yet.");
        return;
    }

    for (const auto& msg : messages) {
        if (msg.is_exec_log) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.4f, 1));
            ImGui::Text("Execution Log:");
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1));
            ImGui::TextWrapped("%s", msg.body.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
            continue;
        }

        if (!msg.body.empty()) {
            bool is_agent = (msg.sender != "user");
            ImVec4 name_color = is_agent
                ? ImVec4(0.4f, 0.7f, 1.0f, 1)
                : ImVec4(0.8f, 0.8f, 0.3f, 1);

            ImGui::TextColored(name_color, "%s", msg.sender.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("  (%s)", msg.timestamp.c_str());

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.9f, 1));
            ImGui::TextWrapped("%s", msg.body.c_str());
            ImGui::PopStyleColor();

            auto at_pos = msg.body.find('@');
            if (at_pos != std::string::npos) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 0.7f), " [@detected]");
            }

            ImGui::Spacing();
        }
    }
}

} // namespace ConversationThread