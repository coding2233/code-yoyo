#pragma once

#include <string>
#include <vector>
#include "models/Task.h"
#include "models/Project.h"
#include "models/Agent.h"
#include "models/Skill.h"

class MarkdownParser {
public:
    // Parse projects index
    static std::vector<Project> ParseProjectsIndex(const std::string& content);
    static std::string FormatProjectsIndex(const std::vector<Project>& projects);

    // Parse project file
    static Project ParseProject(const std::string& content, const std::string& codeyoyo_path);
    static std::string FormatProject(const Project& p);

    // Parse task file
    static Task ParseTask(const std::string& content);
    static std::string FormatTask(const Task& task);

    // Parse agents
    static std::vector<Agent> ParseAgents(const std::string& content);
    static std::string FormatAgents(const std::vector<Agent>& agents);

    // Parse skills
    static std::vector<Skill> ParseSkills(const std::string& content);
    static std::string FormatSkills(const std::vector<Skill>& skills);

    // Merge agents (project overrides global)
    static std::vector<Agent> MergeAgents(
        const std::vector<Agent>& global,
        const std::vector<Agent>& project);

private:
    static std::string Trim(const std::string& s);
    static std::string GetLineValue(const std::string& line, const std::string& key);
    static std::string ExtractBoldValue(const std::string& line, const std::string& key);
    static bool StartsWith(const std::string& s, const std::string& prefix);
    static bool EndsWith(const std::string& s, const std::string& suffix);
};
