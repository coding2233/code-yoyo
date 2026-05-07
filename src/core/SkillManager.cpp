#include "SkillManager.h"
#include "storage/MarkdownParser.h"
#include "core/FileSystem.h"

void SkillManager::Init() {
    LoadSkills();
}

void SkillManager::Shutdown() {
    skills_.clear();
}

void SkillManager::LoadSkills() {
    auto path = FileSystem::GetCodeYoYoDir() + "/skills.md";
    if (!FileSystem::FileExists(path)) {
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