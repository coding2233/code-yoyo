#include "AgentManager.h"
#include "storage/MarkdownParser.h"
#include "core/FileSystem.h"

void AgentManager::Init() {
    LoadAgents();
}

void AgentManager::Shutdown() {
    global_agents_.clear();
}

void AgentManager::LoadAgents() {
    auto path = FileSystem::GetCodeYoYoDir() + "/agents.md";
    if (!FileSystem::FileExists(path)) {
        Agent default_agent;
        default_agent.id = "opencode";
        default_agent.name = "OpenCode";
        default_agent.description = "通用 AI 编码助手";
        default_agent.command = "opencode";
        default_agent.auto_approve = "low";
        default_agent.enabled = true;
        global_agents_.push_back(default_agent);
        SaveAgents();
        return;
    }
    auto content = FileSystem::ReadFile(path);
    global_agents_ = MarkdownParser::ParseAgents(content);
}

void AgentManager::SaveAgents() {
    auto path = FileSystem::GetCodeYoYoDir() + "/agents.md";
    auto content = MarkdownParser::FormatAgents(global_agents_);
    FileSystem::WriteFile(path, content);
}

std::vector<Agent> AgentManager::GetMergedAgents(const std::vector<Agent>& project_overrides) {
    return MarkdownParser::MergeAgents(global_agents_, project_overrides);
}

Agent* AgentManager::FindAgent(const std::string& id) {
    for (auto& a : global_agents_) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

Agent* AgentManager::FindAgentByName(const std::string& name) {
    for (auto& a : global_agents_) {
        if (a.name == name || a.id == name) return &a;
    }
    return nullptr;
}