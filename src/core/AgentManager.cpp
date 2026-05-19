#include "AgentManager.h"
#include "storage/MarkdownParser.h"
#include "core/FileSystem.h"
#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;

void AgentManager::Init() {
    LoadAgents();
    LoadBundledAgents();
}

void AgentManager::Shutdown() {
    global_agents_.clear();
}

static std::string ResolveInstructions(const std::string& raw, const std::string& base_dir) {
    if (raw.size() > 3 && raw.substr(raw.size() - 3) == ".md") {
        auto sep = raw.find_first_of("/\\");
        if (sep == std::string::npos) {
            auto fpath = base_dir + "/" + raw;
            if (FileSystem::FileExists(fpath)) {
                return FileSystem::ReadFile(fpath);
            }
        }
        if (FileSystem::FileExists(raw)) {
            return FileSystem::ReadFile(raw);
        }
    }
    return raw;
}

void AgentManager::LoadAgents() {
    auto base_dir = FileSystem::GetCodeYoYoDir();
    auto path = base_dir + "/agents.md";
    if (!FileSystem::FileExists(path)) {
        Agent default_agent;
        default_agent.id = "opencode";
        default_agent.name = "OpenCode";
        default_agent.description = "通用 AI 编码助手";
        default_agent.command = "opencode";
        default_agent.args = "run --model ${model}";
        default_agent.model = "opencode-go/deepseek-v4-flash";
        default_agent.auto_approve = "low";
        default_agent.enabled = true;
        global_agents_.push_back(default_agent);
        SaveAgents();
        return;
    }
    auto content = FileSystem::ReadFile(path);
    global_agents_ = MarkdownParser::ParseAgents(content);

    for (auto& a : global_agents_) {
        a.instructions = ResolveInstructions(a.instructions, base_dir + "/agents");
    }
}

void AgentManager::LoadBundledAgents() {
    auto configs_dir = FileSystem::GetExeDir() + "/configs/agents";
    std::cerr << "[AgentManager] LoadBundledAgents: looking for " << configs_dir << "\n";
    if (!FileSystem::DirExists(configs_dir)) {
        std::cerr << "[AgentManager] LoadBundledAgents: directory not found\n";
        return;
    }

    for (const auto& entry : fs::directory_iterator(configs_dir)) {
        if (!entry.is_directory()) continue;
        auto agent_dir = entry.path().string();
        auto agent_file = agent_dir + "/agent.md";
        if (!FileSystem::FileExists(agent_file)) {
            std::cerr << "[AgentManager] LoadBundledAgents: no agent.md in " << agent_dir << "\n";
            continue;
        }

        auto content = FileSystem::ReadFile(agent_file);
        std::cerr << "[AgentManager] LoadBundledAgents: reading " << agent_file
                  << " (" << content.size() << " bytes)\n";

        auto parsed = MarkdownParser::ParseAgents(content);
        std::cerr << "[AgentManager] LoadBundledAgents: parsed " << parsed.size() << " agents\n";

        for (auto& a : parsed) {
            std::cerr << "[AgentManager] LoadBundledAgents: agent id='" << a.id
                      << "', args='" << a.args << "', model='" << a.model
                      << "', instructions.size=" << a.instructions.size() << "\n";

            a.instructions = ResolveInstructions(a.instructions, agent_dir);
            bool found = false;
            for (auto& existing : global_agents_) {
                if (existing.id != a.id) continue;
                found = true;
                std::cerr << "[AgentManager] LoadBundledAgents: merging into existing '" << a.id << "'\n";
                if (existing.args.empty() && !a.args.empty()) {
                    existing.args = a.args;
                    std::cerr << "[AgentManager]  -> filled args='" << existing.args << "'\n";
                }
                if (existing.model.empty() && !a.model.empty()) {
                    existing.model = a.model;
                    std::cerr << "[AgentManager]  -> filled model='" << existing.model << "'\n";
                }
                if (existing.instructions.empty() && !a.instructions.empty()) {
                    existing.instructions = a.instructions;
                    std::cerr << "[AgentManager]  -> filled instructions (" << a.instructions.size() << " chars)\n";
                }
                if (existing.command.empty() && !a.command.empty()) {
                    existing.command = a.command;
                    std::cerr << "[AgentManager]  -> filled command='" << existing.command << "'\n";
                }
                break;
            }
            if (!found) {
                std::cerr << "[AgentManager] LoadBundledAgents: adding new agent '" << a.id << "'\n";
                global_agents_.push_back(std::move(a));
            }
        }
    }
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