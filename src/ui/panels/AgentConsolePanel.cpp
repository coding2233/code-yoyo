#include "AgentConsolePanel.h"
#include "ui/LayoutManager.h"
#include "ui/Theme.h"
#include <sstream>

const char* AgentConsolePanel::StatusIcon(const Executor::Execution& exec) const {
    switch (exec.status) {
        case Executor::Execution::Pending:    return "\u25CB";
        case Executor::Execution::Running:    return "\u25C9";
        case Executor::Execution::Completed:  return "\u25CF";
        case Executor::Execution::Failed:     return "\u2715";
        case Executor::Execution::Cancelled:  return "\u2014";
    }
    return "\u25CB";
}

ImU32 AgentConsolePanel::StatusColor(const Executor::Execution& exec) const {
    switch (exec.status) {
        case Executor::Execution::Pending:    return IM_COL32(100, 130, 180, 255);
        case Executor::Execution::Running:    return IM_COL32(60,  160, 220, 255);
        case Executor::Execution::Completed:  return IM_COL32(80,  200, 120, 255);
        case Executor::Execution::Failed:     return IM_COL32(220, 60,  60,  255);
        case Executor::Execution::Cancelled:  return IM_COL32(140, 140, 150, 255);
    }
    return IM_COL32(160, 160, 170, 255);
}

void AgentConsolePanel::Render(const std::vector<Executor::Execution>& executions, const LayoutManager& layout) {
    if (!open_) return;

    auto rect = layout.GetPanelRect(PanelArea::Bottom);
    ImGui::SetNextWindowPos(rect.Min);
    ImGui::SetNextWindowSize(rect.GetSize());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("Agent Console", &open_,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (executions.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1), "No executions yet. Select a subtask and run it.");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    float avail_w = ImGui::GetContentRegionAvail().x;

    ImGui::PushItemWidth(200);
    std::string preview = "Latest";
    if (selected_execution_ >= 0 && selected_execution_ < (int)executions.size()) {
        const auto& e = executions[selected_execution_];
        preview = e.agent_name + " / " + e.subtask_id;
    }
    if (ImGui::BeginCombo("##exec_selector", preview.c_str())) {
        for (int i = 0; i < (int)executions.size(); i++) {
            const auto& e = executions[i];
            std::string label = e.agent_name + " / " + e.subtask_id + " [" + e.id + "]";
            bool is_selected = (selected_execution_ == i);
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                selected_execution_ = i;
            }
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    int idx = selected_execution_;
    if (idx < 0 || idx >= (int)executions.size()) {
        idx = (int)executions.size() - 1;
        selected_execution_ = idx;
    }

    const auto& exec = executions[idx];

    ImGui::SameLine();

    auto status_col = StatusColor(exec);
    ImGui::TextColored(ImVec4(
        ((status_col >> 24) & 0xFF) / 255.0f,
        ((status_col >> 16) & 0xFF) / 255.0f,
        ((status_col >> 8) & 0xFF) / 255.0f, 1),
        "%s  %s", StatusIcon(exec),
        exec.status == Executor::Execution::Pending ? "Pending" :
        exec.status == Executor::Execution::Running ? "Running" :
        exec.status == Executor::Execution::Completed ? "Completed" :
        exec.status == Executor::Execution::Failed ? "Failed" : "Cancelled");

    ImGui::SameLine();
    ImGui::TextDisabled("  |  %s  |  %s", exec.agent_name.c_str(), exec.subtask_id.c_str());

    if (!exec.start_time.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("  |  Start: %s", exec.start_time.c_str());
    }

    ImGui::SameLine(avail_w - 120);
    ImGui::Checkbox("Auto-scroll", &auto_scroll_);

    ImGui::Separator();

    ImGui::TextColored(ImVec4(0.5f, 0.7f, 0.5f, 1), "$ %s  %s", exec.agent_id.c_str(), exec.subtask_title.c_str());

    ImVec2 output_size = ImGui::GetContentRegionAvail();
    output_size.y -= 24;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(14, 14, 18, 255));
    ImGui::BeginChild("##output", output_size, true, ImGuiWindowFlags_HorizontalScrollbar);

    if (!exec.output.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 200, 180, 255));
        ImGui::TextWrapped("%s", exec.output.c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.5f, 1), "Waiting for output...");
    }

    if (auto_scroll_ && exec.status == Executor::Execution::Running) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (exec.status == Executor::Execution::Completed) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.4f, 1), "\u2705  Exit: %d  |  %s", exec.exit_code, exec.end_time.c_str());
    } else if (exec.status == Executor::Execution::Failed) {
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1), "\u274C  Exit: %d  |  %s", exec.exit_code, exec.end_time.c_str());
    } else if (exec.status == Executor::Execution::Cancelled) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1), "\u2014  Cancelled  |  %s", exec.end_time.c_str());
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
