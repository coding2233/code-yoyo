#pragma once

#include <string>
#include <vector>
#include <imgui.h>

class AgentManager;
class SkillManager;
class LayoutManager;

class SettingsPanel {
public:
    void Render(AgentManager& agent_mgr, class SkillManager& skill_mgr, const LayoutManager& layout);
    void SetOpen(bool o) { open_ = o; }
    bool IsOpen() const { return open_; }

private:
    bool open_ = true;
    int selected_tab_ = 0;

    int editing_agent_idx_ = -1;
    char agent_id_buf_[64] = {};
    char agent_name_buf_[64] = {};
    char agent_cmd_buf_[128] = {};
    char agent_args_buf_[128] = {};
    char agent_approve_buf_[16] = {};
    bool agent_enabled_ = true;

    int editing_skill_idx_ = -1;
    char skill_id_buf_[64] = {};
    char skill_desc_buf_[256] = {};
    char skill_inst_buf_[2048] = {};

    void RenderAgentsTab(class AgentManager& agent_mgr);
    void RenderSkillsTab(class SkillManager& skill_mgr);
};