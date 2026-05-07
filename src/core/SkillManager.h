#pragma once

#include <string>
#include <vector>
#include "models/Skill.h"

class SkillManager {
public:
    void Init();
    void Shutdown();

    std::vector<Skill>& GetSkills() { return skills_; }
    Skill* FindSkill(const std::string& id);
    void SaveSkills();

    std::string BuildPrompt(const std::string& agent_instructions,
                            const std::string& skill_id);

private:
    void LoadSkills();
    std::vector<Skill> skills_;
};