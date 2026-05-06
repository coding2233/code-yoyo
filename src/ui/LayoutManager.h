#pragma once

#include <imgui.h>

// Panel area identifiers for manual layout
enum class PanelArea {
    Left,
    Center,
    Right,
    Bottom
};

class LayoutManager {
public:
    void Init();
    void Shutdown();
    void BeginFrame();
    void EndFrame();

    // Get the rect for a specific panel area
    ImRect GetPanelRect(PanelArea area) const;

    // Toggle panel visibility
    void ToggleLeft() { show_left_ = !show_left_; }
    void ToggleRight() { show_right_ = !show_right_; }
    void ToggleBottom() { show_bottom_ = !show_bottom_; }

    bool IsLeftVisible() const { return show_left_; }
    bool IsRightVisible() const { return show_right_; }
    bool IsBottomVisible() const { return show_bottom_; }

private:
    bool initialized_ = false;
    bool show_left_ = true;
    bool show_right_ = true;
    bool show_bottom_ = true;

    // Panel sizes
    float left_width_ = 200.0f;
    float right_width_ = 350.0f;
    float bottom_height_ = 220.0f;
    float topbar_height_ = 36.0f;
};
