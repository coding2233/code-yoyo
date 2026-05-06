#include "LayoutManager.h"

void LayoutManager::Init() {
    initialized_ = true;
}

void LayoutManager::Shutdown() {
    initialized_ = false;
}

void LayoutManager::BeginFrame() {
    // No dockspace needed - panels position themselves
}

void LayoutManager::EndFrame() {
}

ImRect LayoutManager::GetPanelRect(PanelArea area) const {
    auto viewport = ImGui::GetMainViewport();
    ImVec2 vp_min = viewport->Pos;
    ImVec2 vp_max = viewport->Pos + viewport->Size;

    float top_offset = topbar_height_;

    // Calculate available space for content
    float content_top = vp_min.y + top_offset;
    float content_bottom = vp_max.y;

    // Bottom panel
    if (show_bottom_) {
        content_bottom -= bottom_height_;
    }

    float left = vp_min.x;
    float right = vp_max.x;

    // Left panel
    if (show_left_) {
        left += left_width_;
    }

    // Right panel
    if (show_right_) {
        right -= right_width_;
    }

    switch (area) {
        case PanelArea::Left:
            return ImRect(
                ImVec2(vp_min.x, content_top),
                ImVec2(vp_min.x + (show_left_ ? left_width_ : 0), content_bottom)
            );
        case PanelArea::Center:
            return ImRect(
                ImVec2(left, content_top),
                ImVec2(right, content_bottom)
            );
        case PanelArea::Right:
            return ImRect(
                ImVec2(vp_max.x - (show_right_ ? right_width_ : 0), content_top),
                ImVec2(vp_max.x, content_bottom)
            );
        case PanelArea::Bottom:
            return ImRect(
                ImVec2(vp_min.x, vp_max.y - (show_bottom_ ? bottom_height_ : 0)),
                ImVec2(vp_max.x, vp_max.y)
            );
    }
    return ImRect(ImVec2(0, 0), ImVec2(0, 0));
}
