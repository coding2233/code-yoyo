#pragma once

#include <string>
#include <vector>
#include <imgui.h>
#include "core/Executor.h"
#include "ui/LayoutManager.h"

class AgentConsolePanel {
public:
    void Render(const std::vector<Executor::Execution>& executions, const LayoutManager& layout);
    void SetOpen(bool o) { open_ = o; }
    bool IsOpen() const { return open_; }

private:
    bool open_ = true;
    bool auto_scroll_ = true;
    int selected_execution_ = -1;

    void RenderExecutionRow(const Executor::Execution& exec, int index);
    const char* StatusIcon(const Executor::Execution& exec) const;
    ImU32 StatusColor(const Executor::Execution& exec) const;
};
