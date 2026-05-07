# CodeYoYo 剩余功能清单

> [详细实现方案 →](todo.md)

---

| # | 功能 | 阶段 | 状态 | 难度 | 前置 |
|---|------|------|------|------|------|
| 1 | @agent 自动补全 + 触发执行 | P2 | ❌ 未实现 | ⭐⭐ | 无 |
| 2 | ApprovalGate 集成到 Executor + UI | P3 | ❌ 未集成 | ⭐⭐ | 无 |
| 3 | DiffReviewPanel 接入 diff 数据 | P3 | ❌ 未集成 | ⭐ | Executor |
| 4 | Executor 串行/并行 + 依赖解析 | P2 | ⚠️ 部分 | ⭐⭐⭐ | 无 |
| 5 | 看板卡片拖拽 | P3 | ❌ 未实现 | ⭐⭐ | 无 |
| 6 | SkillManager 接入 Executor | P2 | ⚠️ 未调用 | ⭐ | 无 |
| 7 | SettingsPanel Skill 编辑器完善 | P2 | ⚠️ 部分 | ⭐ | 无 |
| 8 | ImGui Docking | P1 | ❌ 未启用 | ⭐⭐⭐ | 无 |
| 9 | AI 辅助 Task 拆解 | P3 | ❌ 未实现 | ⭐⭐⭐ | Agent 执行 |

---

## Phase 2 — Agent 执行

### 1. @agent 自动补全 + 触发执行

当用户在回复输入框中输入 `@` 时弹出 Agent 候选列表，选择后自动触发该 Agent 执行当前 Subtask。

```
用户输入: "帮我看下这段代码 @openco"
                                    ┌─────────────────────┐
                                    │ opencode (OpenCode) │
                                    │ claude  (Claude)    │
                                    │ codex   (Codex)     │
                                    └─────────────────────┘
选择 opencode → 发送 → 自动执行 Subtask
```

**涉及文件：**
- `src/ui/panels/TaskDetailPanel.h` — 添加 autocomplete 状态字段
- `src/ui/panels/TaskDetailPanel.cpp` — 实现弹窗 + 触发逻辑

**关键点：**
- 在 `InputTextMultiline` 的每帧检测最后一个 `@` 位置
- 弹出窗口用 `BeginChild` + `Selectable` 实现，位置在输入框下方
- 选择后替换 `@partial` 为完整 agent_id，不清除已有输入
- 发送时检测 body 中 `@agent_id`，自动调用 `Executor::Execute`

---

### 2. SkillManager 接入 Executor

`SkillManager::BuildPrompt` 已经实现但未被调用。

**涉及文件：**
- `src/core/Executor.h` — 构造参数增加 `SkillManager*`
- `src/core/Executor.cpp` — `BuildPrompt()` 中加入 skill 合并
- `src/main.cpp` — 构造 `Executor` 时传入 `skill_mgr_`

变化：
```diff
- Executor(ProcessManager& pm, TaskManager& tm);
+ Executor(ProcessManager& pm, TaskManager& tm, SkillManager* sm = nullptr);
```

```diff
- static std::string BuildPrompt(const Subtask& sub, const Agent& agent)
+ static std::string BuildPrompt(const Subtask& sub, const Agent& agent,
+                                SkillManager* skill_mgr)
```

---

### 3. SettingsPanel Skill 编辑器完善

当前只能编辑 Skill ID，缺少 description 和 instructions 编辑。

**涉及文件：**
- `src/ui/panels/SettingsPanel.h` — 添加 `skill_desc_buf_[256]`、`skill_inst_buf_[2048]`
- `src/ui/panels/SettingsPanel.cpp` — 改用 `TreeNode` 展开 + 完整编辑弹窗

---

## Phase 3 — 完善

### 4. ApprovalGate 集成到 Executor + UI

核心逻辑已实现（`ApprovalGate.h/cpp`），需要串联到执行流程中：

```
用户点击 Run
  → Executor.Execute()
    → ApprovalGate.NeedsApproval(sub, agent)
      ├── 否 → 直接执行（现有逻辑）
      └── 是 → subtask 设为 review 状态
                → TaskDetailPanel 显示 Approve/Reject 按钮
                → 用户审批后更新状态为 in_progress 或 cancelled
```

**涉及文件：**
- `src/core/Executor.cpp` — `Execute()` 开头加入审批门检查
- `src/ui/panels/TaskDetailPanel.cpp` — review 状态增加审批按钮

**自动审批阈值矩阵（与设计文档一致）：**

| Agent auto_approve | Low Risk | Medium Risk | High Risk |
|-------------------|----------|-------------|-----------|
| `low`             | ✅ 自动   | ⏳ 需审批   | ⏳ 需审批 |
| `medium`          | ✅ 自动   | ✅ 自动     | ⏳ 需审批 |
| `high`            | ✅ 自动   | ✅ 自动     | ✅ 自动  |
| `never`           | ⏳ 需审批 | ⏳ 需审批   | ⏳ 需审批 |

---

### 5. DiffReviewPanel 接入 diff 数据

UI 已实现（着色渲染 + Approve/Reject 按钮），缺少 diff 数据来源。

**数据流：**
```
Executor on_exit
  → 检测项目是否为 git 仓库（.git 目录存在）
  → 执行 git diff 捕获文件变更
  → 通过回调传递给 CodeYoYoApp
  → CodeYoYoApp 设置到 DiffReviewPanel
  → 可选：自动切换到 Diff Review 视图
```

**涉及文件：**
- `src/core/Executor.h` — 添加 `OnDiffAvailable` 回调类型
- `src/core/Executor.cpp` — on_exit 中调用 `GetGitDiff()`
- `src/main.cpp` — 设置回调，管理 `pending_diff_`

---

### 6. 看板卡片拖拽

使用 ImGui 内置的拖拽 API，无需第三方库。

```
DragDropSource ← 卡片: 携带 subtask_id
       ↓
DragDropTarget ← 列 (Pending/In Progress/Review/Completed)
       ↓
MarkdownWriter::UpdateSubtaskStatus(content, subtask_id, new_status, "")
       ↓
下次渲染时看板自动刷新
```

**涉及文件：**
- `src/ui/panels/TaskBoardPanel.cpp` — 卡片加 `BeginDragDropSource`，列加 `BeginDragDropTarget`

**扩展（可选）：**
- 拖拽到 Agent 名上 → 分配 Agent
- 拖拽到垃圾桶 → 删除 Subtask（需确认）

---

### 7. Executor 串行/并行 + 依赖解析

```
Subtask model 已有字段：
  exec_mode: serial | parallel
  depends:   ["ST-001", "ST-003"]
```

**新增队列机制：**

```
Enqueue(task, subtask_id, agent, project)
  → 加入 queue_
  → ProcessQueue()

ProcessQueue()
  → 遍历 queue_
    → CanExecute(subtask)
      → depends 全部在 completed_subtasks_ 中?
      → 串行模式下同一 Task 无其他 Subtask 正在运行?
    → 是 → 执行并移除队列
    → 否 → 跳过，下次轮询
```

**涉及文件：**
- `src/core/Executor.h` — 添加 `queue_`、`completed_subtasks_`、`Enqueue()`、`ProcessQueue()`
- `src/core/Executor.cpp` — 实现依赖解析和队列管理

---

### 8. ImGui Docking

用 ImGui Dockspace 替换手动布局。分两步：

**Step 1：编译支持**
- 确保 imgui 包启用了 `IMGUI_ENABLE_DOCKING`
- xmake 配置可能需要改为源码编译 imgui

**Step 2：代码迁移**
- `LayoutManager` 的 `GetPanelRect` 替换为 `DockSpace` + `DockBuilder`
- 各面板移除 `SetNextWindowPos`/`SetNextWindowSize`
- 改用 `Begin()` 自动停靠 + `DockBuilder` 设置初始布局

**涉及文件：**
- `volt-ui/xmake.lua` — 确保 Docking 编译
- `src/ui/LayoutManager.h` — 新方法 `CreateDockspace()`、`SetupInitialLayout()`
- `src/ui/LayoutManager.cpp` — 实现 Dockspace
- `src/ui/panels/*.cpp` — 移除手动定位代码

---

### 9. AI 辅助 Task 拆解

用户在创建 Task 或点击 "AI Decompose" 时，将 Task 描述发给 Agent，Agent 返回结构化的 Subtask 列表。

**两种方式：**

**方式 A：通过 Agent 执行（推荐）**
```
Exec("opencode", "请将以下需求拆解为子任务：\n" + task_description)
→ 解析返回结果 → 自动创建 Subtask
```

**方式 B：本地规则（fallback）**
```
按段落/句子拆分 → 每个段落作为一个 Subtask
```

**涉及文件：**
- `src/ui/panels/TaskDetailPanel.cpp` — 添加 "AI Decompose" 按钮
- `src/core/Executor.cpp` — 拆解 prompt 专用方法（可选）

---

## 快速开始

对某个功能开始开发时：

```bash
# 1. 确认当前分支
git status

# 2. 阅读详细方案
cat docs/todo.md | grep -A 30 "## <功能名>"

# 3. 查看相关文件
# 用 Read 工具逐文件阅读

# 4. 编译验证
xmake
```