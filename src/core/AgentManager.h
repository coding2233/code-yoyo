#pragma once

#include <string>
#include <vector>
#include "models/Agent.h"
#include "models/Skill.h"

class AgentManager {
public:
    void Init();
    void Shutdown();

    std::vector<Agent>& GetGlobalAgents() { return global_agents_; }
    std::vector<Agent> GetMergedAgents(const std::vector<Agent>& project_overrides);

    Agent* FindAgent(const std::string& id);
    void SaveAgents();

private:
    void LoadAgents();
    std::vector<Agent> global_agents_;
};

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
