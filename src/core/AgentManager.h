#pragma once

#include <string>
#include <vector>
#include "models/Agent.h"

class AgentManager {
public:
    void Init();
    void Shutdown();

    std::vector<Agent>& GetGlobalAgents() { return global_agents_; }
    std::vector<Agent> GetMergedAgents(const std::vector<Agent>& project_overrides);

    Agent* FindAgent(const std::string& id);
    Agent* FindAgentByName(const std::string& name);
    void SaveAgents();

private:
    void LoadAgents();
    void LoadBundledAgents();
    std::vector<Agent> global_agents_;
};
