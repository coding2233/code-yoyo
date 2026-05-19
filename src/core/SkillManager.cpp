#include "SkillManager.h"
#include "storage/MarkdownParser.h"
#include "core/FileSystem.h"
#include <filesystem>
namespace fs = std::filesystem;

void SkillManager::Init() {
    LoadSkills();
    LoadBundledSkills();
}

void SkillManager::Shutdown() {
    skills_.clear();
}

void SkillManager::LoadSkills() {
    auto path = FileSystem::GetCodeYoYoDir() + "/skills.md";
    if (!FileSystem::FileExists(path)) {
        Skill s;
        s.id = "code_generation";
        s.description = "\u6839\u636e\u9700\u6c42\u751f\u6210\u4ee3\u7801";
        s.instructions = "\u6839\u636e\u9700\u6c42\u751f\u6210\u9ad8\u8d28\u91cf\u4ee3\u7801\u3002\n\u9075\u5faa SOLID \u539f\u5219\u3002\n\u5305\u542b\u7c7b\u578b\u5b9a\u4e49\u548c\u9519\u8bef\u5904\u7406\u3002";
        skills_.push_back(s);
        SaveSkills();
        return;
    }
    auto content = FileSystem::ReadFile(path);
    skills_ = MarkdownParser::ParseSkills(content);
}

void SkillManager::LoadBundledSkills() {
    auto configs_dir = FileSystem::GetExeDir() + "/configs/skills";
    if (!FileSystem::DirExists(configs_dir)) return;

    for (const auto& entry : fs::directory_iterator(configs_dir)) {
        if (!entry.is_regular_file()) continue;
        auto fpath = entry.path().string();
        if (fpath.size() < 3 || fpath.substr(fpath.size() - 3) != ".md") continue;

        auto content = FileSystem::ReadFile(fpath);
        auto parsed = MarkdownParser::ParseSkills(content);
        for (auto& s : parsed) {
            bool exists = false;
            for (const auto& existing : skills_) {
                if (existing.id == s.id) { exists = true; break; }
            }
            if (!exists) {
                skills_.push_back(std::move(s));
            }
        }
    }
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
