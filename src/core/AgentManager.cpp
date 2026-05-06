#include "AgentManager.h"
#include "storage/MarkdownParser.h"
#include "core/FileSystem.h"

// ---- AgentManager ----

void AgentManager::Init() {
    LoadAgents();
}

void AgentManager::Shutdown() {
    global_agents_.clear();
}

void AgentManager::LoadAgents() {
    auto path = FileSystem::GetCodeYoYoDir() + "/agents.md";
    if (!FileSystem::FileExists(path)) {
        // Create default agents.md
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

// ---- SkillManager ----

void SkillManager::Init() {
    LoadSkills();
}

void SkillManager::Shutdown() {
    skills_.clear();
}

void SkillManager::LoadSkills() {
    auto path = FileSystem::GetCodeYoYoDir() + "/skills.md";
    if (!FileSystem::FileExists(path)) {
        // Create default skills.md
        Skill s;
        s.id = "code_generation";
        s.description = "根据需求生成代码";
        s.instructions = "根据需求生成高质量代码。\n遵循 SOLID 原则。\n包含类型定义和错误处理。";
        skills_.push_back(s);
        SaveSkills();
        return;
    }
    auto content = FileSystem::ReadFile(path);
    skills_ = MarkdownParser::ParseSkills(content);
}

void SkillManager::SaveSkills() {
    auto path = FileSystem::GetCodeYoYoDir() + "/skills.md";
    auto content = MarkdownParser::FormatSkills(skills_);
    FileSystem::WriteFile(path, content);
}

Skill* SkillManager::FindSkill(const std::string& id) {
    for (auto& s : skills_) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

std::string SkillManager::BuildPrompt(const std::string& agent_instructions,
                                       const std::string& skill_id)
{
    auto* skill = FindSkill(skill_id);
    if (!skill) return agent_instructions;

    std::string result = agent_instructions;
    if (!result.empty() && !skill->instructions.empty()) {
        result += "\n\n---\n\n";
    }
    result += skill->instructions;
    return result;
}
