#pragma once

#include <string>
#include <vector>
#include <imgui.h>

class LayoutManager;

class DiffReviewPanel {
public:
    void Render(const LayoutManager& layout);
    void SetOpen(bool o) { open_ = o; }
    bool IsOpen() const { return open_; }
    void SetDiffContent(const std::string& content) { diff_content_ = content; }

private:
    bool open_ = true;
    std::string diff_content_;
    bool auto_scroll_ = true;
};