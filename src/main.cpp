#include <volt-ui/VoltApp.h>
#include <imgui.h>

#include "core/ProjectManager.h"
#include "core/TaskManager.h"
#include "core/AgentManager.h"
#include "core/SkillManager.h"
#include "core/Executor.h"
#include "core/ProcessManager.h"
#include "core/FileSystem.h"
#include "core/Scheduler.h"
#include "core/ApprovalGate.h"
#include "storage/MarkdownParser.h"
#include "ui/LayoutManager.h"
#include "ui/Theme.h"
#include "ui/panels/ProjectPanel.h"
#include "ui/panels/TaskBoardPanel.h"
#include "ui/panels/TaskTreePanel.h"
#include "ui/panels/TaskDetailPanel.h"
#include "ui/panels/AgentConsolePanel.h"
#include "ui/panels/SchedulePanel.h"
#include "ui/panels/DiffReviewPanel.h"
#include "ui/panels/SettingsPanel.h"

class CodeYoYoApp : public volt::App {
public:
    CodeYoYoApp(const volt::AppConfig& cfg) : volt::App(cfg),
        executor_(process_mgr_, task_mgr_, &skill_mgr_),
        scheduler_(project_mgr_, task_mgr_, executor_, agent_mgr_) {}

protected:
    void OnCreate() override {
        Theme::ApplyDark();

        FileSystem::CreateDirs(FileSystem::GetCodeYoYoDir());
        FileSystem::CreateDirs(FileSystem::GetProjectsDir());

        project_mgr_.Init();
        agent_mgr_.Init();
        skill_mgr_.Init();

        layout_mgr_.Init();

        // Wire up diff callback
        executor_.SetOnDiffAvailable([this](const std::string& diff, const std::string& subtask_id) {
            pending_diff_ = diff;
            pending_diff_subtask_ = subtask_id;
        });

        scheduler_.Start();
    }

    void OnRender() override {
        layout_mgr_.BeginFrame();

        // Show pending diff notification
        if (!pending_diff_.empty()) {
            diff_review_panel_.SetDiffContent(pending_diff_);
        }

        // Left panel: project list
        if (show_project_panel_)
            project_panel_.Render(project_mgr_, layout_mgr_);

        // Center panel: based on current view
        if (current_view_ == 0) {
            task_board_panel_.Render(project_mgr_, task_mgr_, layout_mgr_);
        } else if (current_view_ == 1) {
            task_tree_panel_.Render(project_mgr_, layout_mgr_);
        } else if (current_view_ == 2) {
            schedule_panel_.Render(project_mgr_, task_mgr_, layout_mgr_);
        } else if (current_view_ == 3) {
            diff_review_panel_.Render(layout_mgr_);
        } else if (current_view_ == 4) {
            settings_panel_.Render(agent_mgr_, skill_mgr_, layout_mgr_);
        }

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
        if (show_detail_panel_)
            task_detail_panel_.Render(project_mgr_, task_mgr_, executor_, agent_mgr_, layout_mgr_);

        // Bottom panel: agent console
        auto executions = executor_.GetExecutions();
        if (show_console_panel_)
            agent_console_panel_.Render(executions, layout_mgr_);

        RenderTopbarMenu();

        layout_mgr_.EndFrame();
    }

    void RenderTopbarMenu() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Kanban Board", nullptr, current_view_ == 0)) { current_view_ = 0; }
                if (ImGui::MenuItem("Task Tree", nullptr, current_view_ == 1)) { current_view_ = 1; }
                if (ImGui::MenuItem("Scheduled Tasks", nullptr, current_view_ == 2)) { current_view_ = 2; }
                if (ImGui::MenuItem("Diff Review", !pending_diff_.empty() ? "●" : nullptr, current_view_ == 3)) { current_view_ = 3; }
                if (ImGui::MenuItem("Settings", nullptr, current_view_ == 4)) { current_view_ = 4; }
                ImGui::Separator();
                ImGui::MenuItem("Project Panel", nullptr, &show_project_panel_);
                ImGui::MenuItem("Task Detail", nullptr, &show_detail_panel_);
                ImGui::MenuItem("Agent Console", nullptr, &show_console_panel_);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Tools")) {
                if (ImGui::MenuItem("Scheduler", scheduler_.IsRunning() ? "Running" : "Stopped")) {
                    if (scheduler_.IsRunning()) scheduler_.Stop();
                    else scheduler_.Start();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }

    void OnEvent(const SDL_Event& event) override {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_ESCAPE) {
                Quit();
            }
        }
    }

    void OnDestroy() override {
        scheduler_.Stop();
        process_mgr_.ShutdownAll();
        layout_mgr_.Shutdown();
        skill_mgr_.Shutdown();
        agent_mgr_.Shutdown();
        project_mgr_.Shutdown();
    }

private:
    ProjectManager project_mgr_;
    TaskManager task_mgr_;
    AgentManager agent_mgr_;
    SkillManager skill_mgr_;

    ProcessManager process_mgr_;
    Executor executor_;
    Scheduler scheduler_;

    LayoutManager layout_mgr_;

    ProjectPanel project_panel_;
    TaskBoardPanel task_board_panel_;
    TaskTreePanel task_tree_panel_;
    TaskDetailPanel task_detail_panel_;
    AgentConsolePanel agent_console_panel_;
    SchedulePanel schedule_panel_;
    DiffReviewPanel diff_review_panel_;
    SettingsPanel settings_panel_;

    int current_view_ = 0;
    bool show_project_panel_ = true;
    bool show_detail_panel_ = true;
    bool show_console_panel_ = true;

    std::string pending_diff_;
    std::string pending_diff_subtask_;
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