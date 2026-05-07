#include "SettingsPanel.h"
#include "core/AgentManager.h"
#include "core/SkillManager.h"
#include "ui/LayoutManager.h"
#include <cstring>

void SettingsPanel::Render(AgentManager& agent_mgr, SkillManager& skill_mgr, const LayoutManager& layout) {
    if (!open_) return;

    ImGui::Begin("Settings", &open_,
        ImGuiWindowFlags_NoCollapse);

    const char* tabs[] = { "Agents", "Skills" };
    for (int i = 0; i < 2; i++) {
        if (i > 0) ImGui::SameLine();
        if (ImGui::Button(tabs[i], ImVec2(120, 0))) {
            selected_tab_ = i;
        }
    }
    ImGui::Separator();

    if (selected_tab_ == 0) {
        RenderAgentsTab(agent_mgr);
    } else {
        RenderSkillsTab(skill_mgr);
    }

    ImGui::End();
}

void SettingsPanel::RenderAgentsTab(AgentManager& agent_mgr) {
    auto& agents = agent_mgr.GetGlobalAgents();

    if (ImGui::Button("+ Add Agent", ImVec2(120, 0))) {
        editing_agent_idx_ = -1;
        agent_id_buf_[0] = '\0';
        agent_name_buf_[0] = '\0';
        agent_cmd_buf_[0] = '\0';
        agent_args_buf_[0] = '\0';
        agent_approve_buf_[0] = '\0';
        agent_enabled_ = true;
        ImGui::OpenPopup("Edit Agent");
    }

    ImGui::Separator();

    for (int i = 0; i < (int)agents.size(); i++) {
        auto& a = agents[i];
        ImGui::PushID(i);

        ImVec4 color = a.enabled ? ImVec4(1, 1, 1, 1) : ImVec4(0.5f, 0.5f, 0.5f, 1);
        ImGui::TextColored(color, "%s", a.id.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", a.name.c_str());

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
        if (ImGui::SmallButton("Edit")) {
            editing_agent_idx_ = i;
            strncpy(agent_id_buf_, a.id.c_str(), sizeof(agent_id_buf_) - 1);
            strncpy(agent_name_buf_, a.name.c_str(), sizeof(agent_name_buf_) - 1);
            strncpy(agent_cmd_buf_, a.command.c_str(), sizeof(agent_cmd_buf_) - 1);
            strncpy(agent_args_buf_, a.args.c_str(), sizeof(agent_args_buf_) - 1);
            strncpy(agent_approve_buf_, a.auto_approve.c_str(), sizeof(agent_approve_buf_) - 1);
            agent_enabled_ = a.enabled;
            ImGui::OpenPopup("Edit Agent");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Del")) {
            agents.erase(agents.begin() + i);
            agent_mgr.SaveAgents();
        }

        ImGui::PopID();
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Edit Agent", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("ID", agent_id_buf_, sizeof(agent_id_buf_));
        ImGui::InputText("Name", agent_name_buf_, sizeof(agent_name_buf_));
        ImGui::InputText("Command", agent_cmd_buf_, sizeof(agent_cmd_buf_));
        ImGui::InputText("Args", agent_args_buf_, sizeof(agent_args_buf_));
        ImGui::InputText("Auto Approve (low/medium/high/never)", agent_approve_buf_, sizeof(agent_approve_buf_));
        ImGui::Checkbox("Enabled", &agent_enabled_);

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            std::string id(agent_id_buf_);
            std::string cmd(agent_cmd_buf_);
            if (!id.empty() && !cmd.empty()) {
                Agent a;
                a.id = id;
                a.name = std::string(agent_name_buf_);
                a.command = cmd;
                a.args = std::string(agent_args_buf_);
                a.auto_approve = std::string(agent_approve_buf_);
                if (a.auto_approve.empty()) a.auto_approve = "never";
                a.enabled = agent_enabled_;

                if (editing_agent_idx_ >= 0 && editing_agent_idx_ < (int)agents.size()) {
                    agents[editing_agent_idx_] = a;
                } else {
                    agents.push_back(a);
                }
                agent_mgr.SaveAgents();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void SettingsPanel::RenderSkillsTab(SkillManager& skill_mgr) {
    auto& skills = skill_mgr.GetSkills();

    if (ImGui::Button("+ Add Skill", ImVec2(120, 0))) {
        editing_skill_idx_ = -1;
        skill_id_buf_[0] = '\0';
        skill_desc_buf_[0] = '\0';
        skill_inst_buf_[0] = '\0';
        ImGui::OpenPopup("Edit Skill");
    }

    ImGui::Separator();

    for (int i = 0; i < (int)skills.size(); i++) {
        auto& s = skills[i];
        ImGui::PushID(i);

        if (ImGui::TreeNode(s.id.c_str())) {
            ImGui::TextWrapped("Description: %s", s.description.c_str());
            ImGui::Spacing();
            if (!s.instructions.empty()) {
                ImGui::TextWrapped("Instructions: %s", s.instructions.c_str());
            }

            if (ImGui::Button("Edit")) {
                editing_skill_idx_ = i;
                strncpy(skill_id_buf_, s.id.c_str(), sizeof(skill_id_buf_) - 1);
                strncpy(skill_desc_buf_, s.description.c_str(), sizeof(skill_desc_buf_) - 1);
                strncpy(skill_inst_buf_, s.instructions.c_str(), sizeof(skill_inst_buf_) - 1);
                ImGui::OpenPopup("Edit Skill");
            }
            ImGui::SameLine();
            if (ImGui::Button("Del")) {
                skills.erase(skills.begin() + i);
                skill_mgr.SaveSkills();
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Edit Skill", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Skill ID", skill_id_buf_, sizeof(skill_id_buf_));
        ImGui::InputText("Description", skill_desc_buf_, sizeof(skill_desc_buf_));
        ImGui::InputTextMultiline("Instructions", skill_inst_buf_,
            sizeof(skill_inst_buf_), ImVec2(400, 120));

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            std::string id(skill_id_buf_);
            if (!id.empty()) {
                Skill s;
                s.id = id;
                s.description = std::string(skill_desc_buf_);
                s.instructions = std::string(skill_inst_buf_);

                if (editing_skill_idx_ >= 0 && editing_skill_idx_ < (int)skills.size()) {
                    skills[editing_skill_idx_] = s;
                } else {
                    skills.push_back(s);
                }
                skill_mgr.SaveSkills();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}