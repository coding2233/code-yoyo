# CodeYoYo 剩余功能开发计划

> 基于 `docs/architecture.md` 的检查结果，整理未完成功能的详细开发方案。

---

## 目录

1. [启用 ImGui Docking（Phase 1）](#1-启用-imgui-dockingphase-1)
2. [Executor 串行/并行调度（Phase 2）](#2-executor-串行并行调度phase-2)
3. [@agent 自动补全 + 触发执行（Phase 2）](#3-agent-自动补全--触发执行phase-2)
4. [ApprovalGate 集成（Phase 3）](#4-approvalgate-集成phase-3)
5. [看板卡片拖拽（Phase 3）](#5-看板卡片拖拽phase-3)
6. [DiffReviewPanel 集成（Phase 3）](#6-diffreviewpanel-集成phase-3)
7. [SkillManager 接入 Executor（Phase 2）](#7-skillmanager-接入-executorphase-2)
8. [SettingsPanel Skills 编辑完善（Phase 2）](#8-settingspanel-skills-编辑完善phase-2)
9. [AI 辅助 Task 拆解（Phase 3）](#9-ai-辅助-task-拆解phase-3)

---

## 1. 启用 ImGui Docking（Phase 1）

### 现状

`LayoutManager` 使用手动布局（`GetPanelRect` 计算固定像素区域），没有启用 ImGui Docking。

### 实现方案

#### 1.1 检查 ImGui 版本是否支持 Docking

`volt-ui/xmake.lua` 中 imgui 的配置：

```lua
add_requires("imgui", {configs = {sdl3 = true, sdl3_renderer = true}})
```

需要在 xmake 中确认 imgui 包编译时启用了 `IMGUI_ENABLE_DOCKING`。如果系统包未启用，需要自定义 imgui 构建或使用源码编译。

#### 1.2 main.cpp 启用 Docking

```cpp
void OnCreate() override {
    Theme::ApplyDark();

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // ImGuiConfigFlags_ViewportsEnable 可选项：浮动窗口

    // ... 其他初始化
}
```

#### 1.3 LayoutManager 改用 Dockspace

修改 `LayoutManager::BeginFrame()` 和 `LayoutManager::EndFrame()`：

```cpp
void LayoutManager::BeginFrame() {
    auto viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("DockSpace", nullptr, flags);
    ImGui::PopStyleVar(2);

    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f),
        ImGuiDockNodeFlags_PassthruCentralNode);
}

void LayoutManager::EndFrame() {
    ImGui::End();
}
```

#### 1.4 更新面板 Render 方法

各面板不再通过 `GetPanelRect` 定位，而是直接用 `ImGui::Begin()` 自由停靠：

```cpp
// 每个面板调用 ImGui::Begin() 即可自动停靠
ImGui::Begin("Projects");
// ... 内容
ImGui::End();

ImGui::Begin("Task Board");
// ... 内容
ImGui::End();
```

#### 1.5 初始布局设置（可选）

通过 `ImGui::DockBuilder` 在首次运行时设置默认布局：

```cpp
// 仅在第一次运行时设置
ImGui::DockBuilderRemoveNode(dockspace_id);
ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

ImGuiID dock_left, dock_right, dock_bottom, dock_center;
ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.15f, &dock_left, &dock_center);
ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Right, 0.25f, &dock_right, &dock_center);
ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Down, 0.25f, &dock_bottom, &dock_center);

ImGui::DockBuilderDockWindow("Projects", dock_left);
ImGui::DockBuilderDockWindow("Task Board", dock_center);
ImGui::DockBuilderDockWindow("Task Detail", dock_right);
ImGui::DockBuilderDockWindow("Agent Console", dock_bottom);
ImGui::DockBuilderFinish(dockspace_id);
```

#### 1.6 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `volt-ui/xmake.lua` | 确保 imgui 启用 Docking |
| `src/main.cpp` | 添加 `DockingEnable` 标志 |
| `src/ui/LayoutManager.h` | 移除 `GetPanelRect`，添加 Dockspace 相关方法 |
| `src/ui/LayoutManager.cpp` | 实现 Dockspace 创建和初始布局 |
| `src/ui/panels/*Panel.cpp` | 移除 `SetNextWindowPos`/`SetNextWindowSize`，改为自由 `Begin()` |

---

## 2. Executor 串行/并行调度（Phase 2）

### 现状

`Executor::Execute` 每次调用立即生成一个子进程。`Subtask` 模型中有 `exec_mode`（Serial/Parallel）和 `depends` 字段，但 Executor 完全忽略它们。

### 实现方案

#### 2.1 添加执行队列

```cpp
class Executor {
    // ... 现有代码 ...

    struct QueuedSubtask {
        Task task;           // 包含完整上下文
        std::string subtask_id;
        Agent agent;
        Project project;
    };

    void Enqueue(const Task& task, const std::string& subtask_id,
                 const Agent& agent, const Project& project);
    void ProcessQueue();  // 检查并执行下一个可执行的 Subtask
    bool CanExecute(const Task& task, const std::string& subtask_id);

private:
    std::vector<QueuedSubtask> queue_;
    std::mutex queue_mu_;

    // 已完成的 subtask 追踪（用于依赖解析）
    std::set<std::string> completed_subtasks_;
};
```

#### 2.2 依赖解析逻辑

```cpp
bool Executor::CanExecute(const Task& task, const std::string& subtask_id) {
    auto* sub = task.FindSubtask(subtask_id);
    if (!sub) return false;

    // 检查所有依赖是否已完成
    for (const auto& dep : sub->depends) {
        if (completed_subtasks_.find(dep) == completed_subtasks_.end()) {
            return false; // 前置依赖未完成
        }
    }
    return true;
}
```

#### 2.3 串行执行逻辑

当 `exec_mode == Serial` 时，同一 Task 的 Subtask 按顺序执行：

```cpp
void Executor::Enqueue(...) {
    // 添加到队列
    {
        std::lock_guard<std::mutex> lock(queue_mu_);
        queue_.push_back({task, subtask_id, agent, project});
    }
    ProcessQueue();
}

void Executor::ProcessQueue() {
    std::lock_guard<std::mutex> lock(queue_mu_);
    for (auto it = queue_.begin(); it != queue_.end(); ) {
        if (CanExecute(it->task, it->subtask_id)) {
            // 检查串行约束：同一 Task 的前一个 Subtask 是否正在运行
            bool blocked = false;
            if (HasSerialSubtaskRunning(it->task.id)) {
                blocked = true;
            }
            if (!blocked) {
                DoExecute(it->task, it->subtask_id, it->agent, it->project);
                it = queue_.erase(it);
                continue;
            }
        }
        ++it;
    }
}
```

#### 2.4 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `src/core/Executor.h` | 添加 `Enqueue`、`ProcessQueue`、`CanExecute`、队列成员变量 |
| `src/core/Executor.cpp` | 实现队列管理和依赖解析逻辑 |

---

## 3. @agent 自动补全 + 触发执行（Phase 2）

### 现状

`TaskDetailPanel::RenderConversation` 只在消息体中检测 `@` 字符串并显示 `[@detected]` 标签，没有自动补全或触发执行。

### 实现方案

#### 3.1 输入框 @ 检测

在 `TaskDetailPanel` 的回复输入框中实时检测 `@` 字符：

```cpp
void TaskDetailPanel::Render(ProjectManager& pm, TaskManager& tm,
    Executor& exec, AgentManager& agent_mgr, const LayoutManager& layout)
{
    // ... 现有代码 ...

    // Reply input with @-autocomplete
    ImGui::Text("Reply:");
    ImGui::InputTextMultiline("##reply", reply_input_, sizeof(reply_input_),
        ImVec2(-1, 60), ImGuiInputTextFlags_AllowTabInput);

    // Detect @ for autocomplete
    std::string input(reply_input_);
    auto at_pos = input.rfind('@');
    if (at_pos != std::string::npos) {
        std::string partial = input.substr(at_pos + 1);
        ShowAgentAutocomplete(partial, agent_mgr);
    }

    // ... 发送按钮 ...
}
```

#### 3.2 自动补全弹窗

```cpp
void TaskDetailPanel::ShowAgentAutocomplete(const std::string& partial,
    AgentManager& agent_mgr)
{
    auto& agents = agent_mgr.GetGlobalAgents();

    // 收集匹配的 Agent
    std::vector<Agent*> matches;
    for (auto& a : agents) {
        if (partial.empty() ||
            a.id.find(partial) == 0 ||
            a.name.find(partial) == 0) {
            matches.push_back(&a);
        }
    }

    if (matches.empty()) return;

    // 在输入框下方显示候选列表
    ImGui::SameLine();
    ImGui::BeginChild("##agent_complete", ImVec2(200, 30 * (int)matches.size() + 4),
        true, ImGuiWindowFlags_Tooltip);

    for (auto* a : matches) {
        bool selected = false;
        if (ImGui::Selectable((a->id + " (" + a->name + ")").c_str(), &selected)) {
            // 替换 @partial 为 @agent_id
            auto at_pos = std::string(reply_input_).rfind('@');
            std::string new_input = std::string(reply_input_).substr(0, at_pos + 1)
                + a->id + " ";
            strncpy(reply_input_, new_input.c_str(), sizeof(reply_input_) - 1);
            show_autocomplete_ = false;
        }
    }

    ImGui::EndChild();
}
```

#### 3.3 发送后自动触发

在发送回复时检测 `@agent` 并自动触发执行：

```cpp
// 在 TaskDetailPanel 的发送逻辑中
void TaskDetailPanel::HandleAgentTrigger(const std::string& body,
    Subtask& sub, Task& task, Executor& exec,
    AgentManager& agent_mgr, const Project& project)
{
    auto at_pos = body.find('@');
    if (at_pos == std::string::npos) return;

    auto after_at = body.substr(at_pos + 1);
    // 提取 agent ID（直到空格或结束）
    auto space = after_at.find(' ');
    std::string agent_id = (space == std::string::npos) ? after_at : after_at.substr(0, space);

    auto* agent = agent_mgr.FindAgent(agent_id);
    if (!agent || !agent->enabled) return;

    // 自动触发执行
    exec.Execute(task, sub.id, *agent, project);
}
```

#### 3.4 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `src/ui/panels/TaskDetailPanel.h` | 添加 `ShowAgentAutocomplete`、`HandleAgentTrigger`、autocomplete 状态 |
| `src/ui/panels/TaskDetailPanel.cpp` | 实现 @ 检测、自动补全弹窗、触发执行 |

---

## 4. ApprovalGate 集成（Phase 3）

### 现状

`ApprovalGate` 类和核心逻辑（`AssessRisk`、`NeedsApproval`）已完成，但 `Executor::Execute` 和 UI 均未调用它。

### 实现方案

#### 4.1 Executor 中集成审批门

```cpp
void Executor::Execute(const Task& task, const std::string& subtask_id,
    const Agent& agent, const Project& project)
{
    auto* sub = task.FindSubtask(subtask_id);
    if (!sub) return;

    // 审批门检查
    ApprovalGate gate;
    if (gate.NeedsApproval(*sub, agent)) {
        // 不立即执行，设置 status = review，等待审批
        auto task_path = project.TaskFilePath(task.id, task.title);
        auto content = FileSystem::ReadFile(task_path);
        if (!content.empty()) {
            auto updated = MarkdownWriter::UpdateSubtaskStatus(
                content, subtask_id, "review",
                "Waiting for approval (risk: " +
                std::string(ApprovalGate::RiskToString(
                    static_cast<ApprovalGate::RiskLevel>(sub->risk))) + ")");
            FileSystem::WriteFile(task_path, updated);
        }

        // 触发审批回调
        if (on_pending_review_) {
            on_pending_review_(subtask_id);
        }
        return;
    }

    // 无需审批，直接执行（现有逻辑）
    // ...
}
```

#### 4.2 UI 审批按钮

在 `TaskDetailPanel` 中，当 subtask 状态为 `review` 时显示审批按钮：

```cpp
if (sub.status == SubtaskStatus::Review) {
    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1), "⚠ Pending Approval");
    ImGui::SameLine();
    ImGui::TextDisabled("(risk: %s)", risk_str.c_str());

    if (ImGui::Button("Approve", ImVec2(100, 0))) {
        auto task_path = project.TaskFilePath(task.id, task.title);
        auto content = FileSystem::ReadFile(task_path);
        if (!content.empty()) {
            auto updated = MarkdownWriter::UpdateSubtaskStatus(
                content, sub.id, "in_progress", "");
            FileSystem::WriteFile(task_path, updated);
        }
        tm.AppendAudit(project->AuditFilePath(), tm.Now(),
            "approval_confirm", "user",
            sub.id + " approved");
    }
    ImGui::SameLine();
    if (ImGui::Button("Reject", ImVec2(100, 0))) {
        auto task_path = project.TaskFilePath(task.id, task.title);
        auto content = FileSystem::ReadFile(task_path);
        if (!content.empty()) {
            auto updated = MarkdownWriter::UpdateSubtaskStatus(
                content, sub.id, "cancelled", "rejected by user");
            FileSystem::WriteFile(task_path, updated);
        }
        tm.AppendAudit(project->AuditFilePath(), tm.Now(),
            "approval_reject", "user",
            sub.id + " rejected");
    }
}
```

#### 4.3 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `src/core/Executor.h` | 添加 `ApprovalGate` 成员和 `on_pending_review_` 回调 |
| `src/core/Executor.cpp` | 在 `Execute` 中加入审批门检查 |
| `src/ui/panels/TaskDetailPanel.cpp` | 添加 review 状态的审批/拒绝按钮 |

---

## 5. 看板卡片拖拽（Phase 3）

### 现状

`TaskBoardPanel` 的卡片是静态的，只能通过点击选中，然后到详情面板操作。

### 实现方案

#### 5.1 拖拽源

```cpp
void TaskBoardPanel::RenderCard(const Subtask& sub) {
    ImGui::PushID(sub.id.c_str());

    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);

    ImU32 card_bg = IM_COL32(22, 22, 28, 220);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, card_bg);
    ImGui::BeginChild(("card" + sub.id).c_str(), ImVec2(0, 80), true);

    // 卡片内容
    ImGui::Text("%s", sub.title.c_str());
    ImGui::Spacing();
    ImGui::TextDisabled("%s", sub.id.c_str());
    ImGui::SameLine();
    // ... risk, assignee ...

    // 拖拽源
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("DND_SUBTASK", sub.id.c_str(),
            sub.id.size() + 1);
        ImGui::Text("Move %s", sub.id.c_str());
        ImGui::EndDragDropSource();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::EndGroup();

    ImGui::PopID();
}
```

#### 5.2 拖拽目标（列）

```cpp
void TaskBoardPanel::RenderKanbanColumn(const std::string& title,
    const std::string& status_str,
    const std::vector<Subtask>& subtasks,
    SubtaskStatus filter_status)
{
    // ... 现有列渲染代码 ...

    ImGui::BeginChild(("body" + title).c_str(), ImVec2(col_width, body_height), true);

    for (auto& sub : subtasks) {
        RenderCard(sub);
    }

    // 列作为拖拽目标
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_SUBTASK")) {
            std::string dropped_id((const char*)payload->Data);

            // 更新 subtask 状态到目标列
            auto* project = /* 当前项目 */;
            auto task_files = FileSystem::ListFiles(project->tasks_path, ".md");
            for (const auto& f : task_files) {
                auto content = FileSystem::ReadFile(f);
                if (content.empty()) continue;
                auto updated = MarkdownWriter::UpdateSubtaskStatus(
                    content, dropped_id, status_str, "");
                if (updated != content) {
                    FileSystem::WriteFile(f, updated);
                    break;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::EndChild();
}
```

#### 5.3 拖到 Agent 上分配

卡片可拖到 Agent 名上实现分配：

```cpp
// 在卡片区域增加 Agent 拖拽目标
if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_AGENT")) {
        std::string agent_id((const char*)payload->Data);
        // 更新 subtask 的 assignee
        UpdateAssignee(dropped_id, agent_id);
    }
    ImGui::EndDragDropTarget();
}
```

#### 5.4 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `src/ui/panels/TaskBoardPanel.h` | 添加拖拽处理方法 |
| `src/ui/panels/TaskBoardPanel.cpp` | 实现 `BeginDragDropSource`/`BeginDragDropTarget` |

---

## 6. DiffReviewPanel 集成（Phase 3）

### 现状

`DiffReviewPanel` UI 已实现，能渲染着色 diff 内容。但没有任何代码向它投递 diff。

### 实现方案

#### 6.1 Executor 中捕获 diff

在 Agent 执行完成后，自动收集 git diff：

```cpp
void Executor::SpawnProcess(...) {
    // ... 现有 on_exit 回调 ...

    auto on_exit = [this, payload](const std::string& pid, int code) {
        // ... 现有逻辑 ...

        // 收集 diff（如果项目是 git 仓库）
        if (code == 0) {
            std::string diff = GetGitDiff(payload->project_repo);
            if (!diff.empty() && on_diff_available_) {
                on_diff_available_(diff, payload->subtask_id);
            }
        }
    };
}

std::string Executor::GetGitDiff(const std::string& repo_path) {
    std::string result;

    // 检查是否是 git 仓库
    std::string git_dir = repo_path + "/.git";
    if (!fs::exists(git_dir)) return "";

    // 执行 git diff
    std::array<char, 4096> buf;
    std::string cmd = "cd " + repo_path + " && git diff";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    while (fgets(buf.data(), buf.size(), pipe) != nullptr) {
        result += buf.data();
    }
    pclose(pipe);

    return result;
}
```

#### 6.2 UI 联动

```cpp
// 在 CodeYoYoApp::OnRender() 中
void OnRender() override {
    // ...

    // 如果有新的 diff，更新 DiffReviewPanel
    if (!pending_diff_.empty()) {
        diff_review_panel_.SetDiffContent(pending_diff_);
        current_view_ = 3; // 切换到 Diff Review 视图
        pending_diff_.clear();
    }

    // ...
}
```

#### 6.3 Executor 回调

```cpp
// 在 main.cpp 创建 Executor 后设置回调
executor_.SetOnDiffAvailable([this](const std::string& diff, const std::string& subtask_id) {
    pending_diff_ = diff;
});
```

#### 6.4 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `src/core/Executor.h` | 添加 `OnDiffAvailable` 回调和 `GetGitDiff` |
| `src/core/Executor.cpp` | 在 on_exit 中调用 git diff |
| `src/main.cpp` | 设置回调，切换到 diff 视图 |

---

## 7. SkillManager 接入 Executor（Phase 2）

### 现状

`SkillManager::BuildPrompt` 已实现但 `Executor` 内部的 `BuildPrompt` 静态函数没有调用它。

### 实现方案

#### 7.1 Executor 接收 SkillManager 引用

```cpp
class Executor {
public:
    Executor(ProcessManager& pm, TaskManager& tm, SkillManager* sm = nullptr);

private:
    SkillManager* skill_mgr_ = nullptr;
};
```

#### 7.2 修改 Prompt 构建

```cpp
// Executor.cpp
static std::string BuildPrompt(const Subtask& sub, const Agent& agent,
                                SkillManager* skill_mgr) {
    std::string prompt;
    if (!agent.instructions.empty()) {
        prompt += agent.instructions + "\n\n";
    }

    // 如果 Agent 关联了 Skill，合并 Skill 指令
    if (skill_mgr && !agent.skills.empty()) {
        for (const auto& skill_id : agent.skills) {
            prompt = skill_mgr->BuildPrompt(prompt, skill_id);
        }
    }

    if (!sub.description.empty()) {
        prompt += sub.description;
    }
    if (prompt.empty()) {
        prompt = sub.title;
    }
    return prompt;
}
```

#### 7.3 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `src/core/Executor.h` | 添加 `SkillManager*` 成员 |
| `src/core/Executor.cpp` | `BuildPrompt` 接收和使用 `SkillManager` |
| `src/main.cpp` | 创建 `Executor` 时传入 `SkillManager` |

---

## 8. SettingsPanel Skills 编辑完善（Phase 2）

### 现状

SettingsPanel 的 Skills 选项卡只能编辑 ID，description 和 instructions 未暴露。

### 实现方案

```cpp
void SettingsPanel::RenderSkillsTab(SkillManager& skill_mgr) {
    // ... 现有按钮 ...

    for (int i = 0; i < (int)skills.size(); i++) {
        auto& s = skills[i];
        ImGui::PushID(i);

        // 点击展开详情
        if (ImGui::TreeNode(s.id.c_str())) {
            ImGui::Text("Description: %s", s.description.c_str());

            // 编辑按钮
            if (ImGui::Button("Edit")) {
                editing_skill_idx_ = i;
                strncpy(skill_id_buf_, s.id.c_str(), sizeof(skill_id_buf_) - 1);
                strncpy(skill_desc_buf_, s.description.c_str(), sizeof(skill_desc_buf_) - 1);
                strncpy(skill_inst_buf_, s.instructions.c_str(), sizeof(skill_inst_buf_) - 1);
                ImGui::OpenPopup("Edit Skill Detail");
            }
            ImGui::SameLine();
            if (ImGui::Button("Del")) {
                skills.erase(skills.begin() + i);
                skill_mgr.SaveSkills();
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    // 详细编辑弹窗
    if (ImGui::BeginPopupModal("Edit Skill Detail", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("ID", skill_id_buf_, sizeof(skill_id_buf_));
        ImGui::InputText("Description", skill_desc_buf_, sizeof(skill_desc_buf_));
        ImGui::InputTextMultiline("Instructions", skill_inst_buf_,
            sizeof(skill_inst_buf_), ImVec2(400, 150));

        if (ImGui::Button("Save")) {
            if (editing_skill_idx_ >= 0 && editing_skill_idx_ < (int)skills.size()) {
                skills[editing_skill_idx_].id = skill_id_buf_;
                skills[editing_skill_idx_].description = skill_desc_buf_;
                skills[editing_skill_idx_].instructions = skill_inst_buf_;
                skill_mgr.SaveSkills();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
```

#### 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `src/ui/panels/SettingsPanel.h` | 添加 `skill_desc_buf_` 和 `skill_inst_buf_` |
| `src/ui/panels/SettingsPanel.cpp` | 实现 Skill 详情编辑 |

---

## 9. AI 辅助 Task 拆解（Phase 3）

### 现状

Task 拆解完全手动：用户创建 Task 后得到一个空的初始 Subtask。

### 实现方案

#### 9.1 Task 创建时触发拆解

```cpp
void TaskBoardPanel::HandleTaskCreated(ProjectManager& pm,
    TaskManager& tm, Task& task, const Project& project)
{
    std::string description = task.description;

    // 如果有默认 agent，将描述发给 agent 请求拆解
    auto* project_pm = /* active project manager */;
    auto* default_agent = agent_mgr.FindAgent("opencode");
    if (default_agent && default_agent->enabled) {
        // 创建一个拆解用的 prompt
        std::string decompose_prompt =
            "请将以下需求拆解为多个子任务（Subtask），"
            "每个子任务包含 ID、标题、描述、风险级别（low/medium/high）。"
            "请以 Markdown 列表格式返回：\n\n"
            "- ST-001: <title>\n  - description: <desc>\n  - risk: <level>\n\n"
            + description;

        // TODO: 调用 Executor 执行拆解 prompt
        // TODO: 解析返回结果，自动创建 Subtask
    }
}
```

#### 9.2 简单的本地拆解（AI 不可用时的 fallback）

```cpp
std::vector<Subtask> DecomposeTask(const std::string& description) {
    std::vector<Subtask> subs;

    // 尝试按段落/句子拆分
    std::istringstream stream(description);
    std::string line;
    int count = 1;
    while (std::getline(stream, line)) {
        if (line.size() > 20) { // 至少 20 字符才算一个有效描述
            Subtask sub;
            sub.id = "ST-" + std::to_string(count);
            sub.title = line.size() > 40 ? line.substr(0, 40) + "..." : line;
            sub.description = line;
            sub.risk = RiskLevel::Medium;
            subs.push_back(sub);
            count++;
        }
    }

    // 如果描述太短，创建一个默认 subtask
    if (subs.empty()) {
        Subtask sub;
        sub.id = "ST-001";
        sub.title = "Implement " + description;
        sub.description = description;
        sub.risk = RiskLevel::Medium;
        subs.push_back(sub);
    }

    return subs;
}
```

#### 9.3  UI 按钮

```cpp
// TaskDetailPanel 中的拆解按钮
if (sub.status == SubtaskStatus::Pending &&
    current_task_->subtasks.size() <= 1) {
    if (ImGui::Button("AI Decompose", ImVec2(120, 0))) {
        // 触发 AI 拆解
        TriggerDecompose(*current_task_, *project, exec, agent_mgr);
    }
}
```

#### 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `src/ui/panels/TaskDetailPanel.h` | 添加 `TriggerDecompose` 方法 |
| `src/ui/panels/TaskDetailPanel.cpp` | 实现 AI 拆解 UI 和逻辑 |

---

## 优先级建议

| 优先级 | 功能 | 预估工作量 | 理由 |
|--------|------|-----------|------|
| **P0** | @agent 自动补全 + 触发 | 2-3h | Phase 2 核心体验，用户交互瓶颈 |
| **P0** | ApprovalGate 集成 + UI | 2-3h | Phase 3 核心，安全审查不可少 |
| **P1** | DiffReviewPanel 集成 | 2-3h | Phase 3 核心，需要看到实际 diff |
| **P1** | Executor 串/并行调度 | 3-4h | 依赖解析是多 Subtask 协作的前提 |
| **P2** | 看板卡片拖拽 | 2-3h | UX 提升，不是核心功能 |
| **P2** | SkillManager 接入 | 1h | 简单集成 |
| **P2** | SettingsPanel Skills 编辑 | 1h | 小修补 |
| **P3** | ImGui Docking | 3-4h | 纯 UI，不影响功能 |
| **P3** | AI Task 拆解 | 4-6h | 依赖 Agent 执行能力，功能增强 |