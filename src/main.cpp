#include <volt-ui/VoltApp.h>
#include <imgui.h>

#include "core/ProjectManager.h"
#include "core/TaskManager.h"
#include "core/AgentManager.h"
#include "core/Executor.h"
#include "core/ProcessManager.h"
#include "core/FileSystem.h"
#include "storage/MarkdownParser.h"
#include "ui/LayoutManager.h"
#include "ui/Theme.h"
#include "ui/panels/ProjectPanel.h"
#include "ui/panels/TaskBoardPanel.h"
#include "ui/panels/TaskDetailPanel.h"
#include "ui/panels/AgentConsolePanel.h"

class CodeYoYoApp : public volt::App {
public:
    CodeYoYoApp(const volt::AppConfig& cfg) : volt::App(cfg),
        executor_(process_mgr_, task_mgr_) {}

protected:
    void OnCreate() override {
        Theme::ApplyDark();

        FileSystem::CreateDirs(FileSystem::GetCodeYoYoDir());
        FileSystem::CreateDirs(FileSystem::GetProjectsDir());

        project_mgr_.Init();
        task_mgr_.Init();
        agent_mgr_.Init();
        skill_mgr_.Init();

        layout_mgr_.Init();
    }

    void OnRender() override {
        layout_mgr_.BeginFrame();

        // Left panel: project list
        project_panel_.Render(project_mgr_, layout_mgr_);

        // Center panel: kanban board
        task_board_panel_.Render(project_mgr_, task_mgr_, layout_mgr_);

        // Sync selected subtask to detail panel
        auto* selected = task_board_panel_.GetSelectedSubtask();
        auto* project = project_mgr_.GetActiveProject();
        if (selected && project) {
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

        // Right panel: task detail
        task_detail_panel_.Render(project_mgr_, task_mgr_, executor_, agent_mgr_, layout_mgr_);

        // Bottom panel: agent console
        auto executions = executor_.GetExecutions();
        agent_console_panel_.Render(executions, layout_mgr_);

        layout_mgr_.EndFrame();
    }

    void OnEvent(const SDL_Event& event) override {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_ESCAPE) {
                Quit();
            }
        }
    }

    void OnDestroy() override {
        process_mgr_.ShutdownAll();
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

    ProcessManager process_mgr_;
    Executor executor_;

    LayoutManager layout_mgr_;
    ProjectPanel project_panel_;
    TaskBoardPanel task_board_panel_;
    TaskDetailPanel task_detail_panel_;
    AgentConsolePanel agent_console_panel_;
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
