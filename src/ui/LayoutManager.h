#pragma once

#include <imgui.h>

class LayoutManager {
public:
    void Init();
    void Shutdown();
    void BeginDockspace();
    void EndDockspace();

private:
    bool initialized_ = false;
    ImGuiID dockspace_id_ = 0;
};
