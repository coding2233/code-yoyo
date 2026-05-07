#pragma once

#include <imgui.h>
#include <imgui_internal.h>

class LayoutManager {
public:
    void Init();
    void Shutdown();
    void BeginFrame();
    void EndFrame();

    void ToggleLeft() {}
    void ToggleRight() {}
    void ToggleBottom() {}

    void SetMenuBarHeight(float h) { menubar_height_ = h; }
    bool IsLeftVisible() const { return true; }
    bool IsRightVisible() const { return true; }
    bool IsBottomVisible() const { return true; }

    void ResetLayout() { reset_layout_ = true; }
    bool IsResetting() const { return reset_layout_; }

private:
    bool reset_layout_ = false;
    bool initial_layout_done_ = false;
    float menubar_height_ = 0.0f;

    void SetupInitialLayout();
};
