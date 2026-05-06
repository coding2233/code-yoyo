#include <volt-ui/VoltApp.h>
#include <imgui.h>

#include "core/ProjectManager.h"
#include "core/TaskManager.h"
#include "core/AgentManager.h"
#include "core/FileSystem.h"
#include "storage/MarkdownParser.h"
#include "ui/LayoutManager.h"
#include "ui/Theme.h"
#include "ui/panels/ProjectPanel.h"
#include "ui/panels/TaskBoardPanel.h"
#include "ui/panels/TaskDetailPanel.h"

class CodeYoYoApp : public volt::App {
public:
    CodeYoYoApp(const volt::AppConfig& cfg) : volt::App(cfg) {}

protected:
    void OnCreate() override {
        // Enable ImGui Docking
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // Apply theme
        Theme::ApplyDark();

        // Init managers
        FileSystem::CreateDirs(FileSystem::GetCodeYoYoDir());
        FileSystem::CreateDirs(FileSystem::GetProjectsDir());

        project_mgr_.Init();
        task_mgr_.Init();
        agent_mgr_.Init();
        skill_mgr_.Init();

        layout_mgr_.Init();
    }

    void OnRender() override {
        // Begin dockspace
        layout_mgr_.BeginDockspace();

        // Render panels
        project_panel_.Render(project_mgr_);

        task_board_panel_.Render(project_mgr_, task_mgr_);

        // Sync selected subtask to detail panel
        auto* selected = task_board_panel_.GetSelectedSubtask();
        if (selected) {
            auto* project = project_mgr_.GetActiveProject();
            if (project) {
                auto task_files = FileSystem::ListFiles(project->tasks_path, ".md");
                for (const auto& f : task_files) {
                    auto content = FileSystem::ReadFile(f);
                    if (content.empty()) continue;
                    auto task = MarkdownParser::ParseTask(content);
                    for (auto& sub : task.subtasks) {
                        if (sub.id == selected->id) {
                            task_detail_panel_.SetTask(&task);
                            task_detail_panel_.SetSubtask(&sub);
                            break;
                        }
                    }
                }
            }
        }

        task_detail_panel_.Render(project_mgr_, task_mgr_);

        layout_mgr_.EndDockspace();
    }

    void OnEvent(const SDL_Event& event) override {
        // Handle global shortcuts
        if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_ESCAPE) {
                Quit();
            }
        }
    }

    void OnDestroy() override {
        layout_mgr_.Shutdown();
        skill_mgr_.Shutdown();
        agent_mgr_.Shutdown();
        task_mgr_.Shutdown();
        project_mgr_.Shutdown();
    }

private:
    ProjectManager project_mgr_;
    TaskManager task_mgr_;
    AgentManager agent_mgr_;
    SkillManager skill_mgr_;

    LayoutManager layout_mgr_;
    ProjectPanel project_panel_;
    TaskBoardPanel task_board_panel_;
    TaskDetailPanel task_detail_panel_;
};

int main() {
    volt::AppConfig cfg;
    cfg.title = "CodeYoYo";
    cfg.width = 1400;
    cfg.height = 860;
    cfg.use_topbar = true;
    CodeYoYoApp app(cfg);
    return app.Run();
}
