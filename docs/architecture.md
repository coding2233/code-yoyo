# CodeYoYo 架构设计文档

> CodeYoYo — 本地 AI 编码任务编排桌面应用
> 基于 volt-ui (SDL3 + ImGui + LeanCLR) 框架

---

## 1. 项目定位

CodeYoYo 是一个**本地 AI 编码任务编排桌面应用**。它不直接提供 AI 能力，而是作为一个 **Orchestrator（编排器）**，调度现有的 AI 编码 CLI 工具（如 OpenCode、Claude Code、Codex、CodeBuddy 等）来执行代码任务。

核心概念：**以 Project 为主体，将需求拆解为 Task/Subtask，分配给 Agent 执行，追踪状态，支持人工审批和 Agent 间协作。**

### 1.1 设计原则

- **本地优先**：所有数据存储在本地文件系统，以 Markdown 格式存储，人类可读可编辑
- **工具无关**：不绑定特定 AI 工具，通过 CLI 子进程 + PTY 交互
- **可审计**：所有操作记录审计日志，支持回溯
- **渐进复杂**：从看板 + 任务管理起步，逐步增加自动化能力

---

## 2. 架构总览

```
┌────────────────────────────────────────────────────────────┐
│                     CodeYoYo (C++ + ImGui)                  │
├────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │                  UI Layer                             │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────┐  │  │
│  │  │Project   │ │TaskBoard │ │TaskDetail│ │Schedule│  │  │
│  │  │Panel     │ │(看板/树形)│ │(对话区)   │ │Panel   │  │  │
│  │  └──────────┘ └──────────┘ └──────────┘ └────────┘  │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────────────┐     │  │
│  │  │AgentConf │ │Agent     │ │DiffReview +      │     │  │
│  │  │Panel     │ │Console   │ │ApprovalGate      │     │  │
│  │  └──────────┘ └──────────┘ └──────────────────┘     │  │
│  └──────────────────────────────────────────────────────┘  │
│                              │                              │
│  ┌───────────────────────────┴──────────────────────────┐  │
│  │                    Core Layer                         │  │
│  │  ┌───────────┐ ┌───────────┐ ┌─────────────────┐    │  │
│  │  │Project    │ │Task       │ │Scheduler        │    │  │
│  │  │Manager    │ │Manager    │ │(定时检查+触发)    │    │  │
│  │  ├───────────┤ ├───────────┤ ├─────────────────┤    │  │
│  │  │Agent      │ │Skill      │ │ProcessManager   │    │  │
│  │  │Manager    │ │Manager    │ │(子进程/PTY/管道)  │    │  │
│  │  └───────────┘ └───────────┘ └─────────────────┘    │  │
│  │  ┌───────────┐ ┌───────────┐ ┌─────────────────┐    │  │
│  │  │Executor   │ │Approval   │ │MarkdownParser   │    │  │
│  │  │(并/串行调度)│ │Gate       │ │(.md ↔ Model)    │    │  │
│  │  └───────────┘ └───────────┘ └─────────────────┘    │  │
│  └──────────────────────────────────────────────────────┘  │
└────────────────────────────┬───────────────────────────────┘
                             │
                             ▼
              ┌──────────────────────────┐
              │  外部 AI CLI Tools        │
              │  opencode / claude /     │
              │  codex / codebuddy ...   │
              └──────────────────────────┘
```

### 2.1 Layer 职责

| 层级 | 职责 |
|------|------|
| **UI Layer** | ImGui 面板渲染、用户交互、拖拽、输入处理 |
| **Core Layer** | 业务逻辑、数据管理、进程调度、定时任务 |
| **External** | 外部 AI CLI 工具，通过子进程调用 |

### 2.2 数据流

```
用户操作 → Controller → Model (Markdown 文件) → UI 刷新
                            │
Agent 执行 → ProcessManager → Executor → 更新 Model
                            │
Scheduler (定时) → Executor → Agent 执行
```

---

## 3. 目录与文件结构

### 3.1 全局目录

所有数据均存储在 `~/.codeyoyo/` 目录下：

```
~/.codeyoyo/
├── projects.md              # 所有项目的索引
├── config.md                # 全局配置
├── agents.md                # 全局 Agent 定义（所有项目可见）
├── skills.md                # 全局 Skill 定义（所有项目可见）
│
└── projects/
    ├── <project-name>/      # 每个项目一个独立目录
    │   ├── project.md       # 项目元信息 + 关联仓库路径
    │   ├── agents.md        # 可选：项目级 Agent 覆盖/扩展
    │   ├── AUDIT.md         # 审计日志（按时间追加写入）
    │   ├── tasks/           # 每个 Task 一个 .md 文件
    │   │   ├── UX-001-实现用户登录功能.md
    │   │   ├── UX-002-添加仪表盘页面.md
    │   │   └── UX-003-每日依赖更新检查.md
    │   └── sessions/        # Agent 执行原始输出
    │       └── UX-001/
    │           ├── ST-001.log
    │           └── ST-002.log
    │
    ├── <another-project>/
    └── ...
```

### 3.2 应用代码目录

```
CodeYoYo/
├── src/
│   ├── main.cpp                        # 入口
│   │
│   ├── app/
│   │   ├── CodeYoYoApp.h/cpp           # App 子类，生命周期管理
│   │   ├── LayoutManager.h/cpp         # UI Docking 布局管理
│   │   └── Theme.h/cpp                 # 主题/样式定义
│   │
│   ├── models/
│   │   ├── Project.h/cpp               # 项目数据结构
│   │   ├── Task.h/cpp                  # Task / Subtask 数据结构
│   │   ├── Agent.h/cpp                 # Agent 配置数据结构
│   │   └── Skill.h/cpp                 # Skill 数据结构
│   │
│   ├── storage/
│   │   ├── MarkdownParser.h/cpp        # Markdown → Model 解析器
│   │   ├── MarkdownWriter.h/cpp        # Model → Markdown 序列化
│   │   └── FileSystem.h/cpp            # 文件操作封装
│   │
│   ├── core/
│   │   ├── ProjectManager.h/cpp        # 项目 CRUD、切换、索引
│   │   ├── TaskManager.h/cpp           # Task/Subtask 管理 + 状态机
│   │   ├── AgentManager.h/cpp          # Agent 注册、查询、合并
│   │   ├── SkillManager.h/cpp          # Skill 注册、查询
│   │   ├── Executor.h/cpp              # 执行引擎（Subtask → Agent 调度）
│   │   ├── ProcessManager.h/cpp        # 子进程管理（ConPTY/Pipe）
│   │   ├── ApprovalGate.h/cpp          # 审批门逻辑
│   │   └── Scheduler.h/cpp             # 定时任务引擎（Cron 轮询）
│   │
│   └── ui/
│       ├── panels/
│       │   ├── ProjectPanel.h/cpp       # 左侧项目列表
│       │   ├── TaskBoardPanel.h/cpp     # 看板视图（默认）
│       │   ├── TaskTreePanel.h/cpp      # 树形列表视图
│       │   ├── TaskDetailPanel.h/cpp    # Task/Subtask 详情 + 对话区
│       │   ├── AgentConsolePanel.h/cpp  # 底部 Agent 输出控制台
│       │   ├── SchedulePanel.h/cpp      # 定时任务管理面板
│       │   ├── DiffReviewPanel.h/cpp    # Diff 审查弹窗
│       │   └── SettingsPanel.h/cpp      # 设置面板（Agent/Skill 管理）
│       │
│       └── widgets/
│           ├── KanbanColumn.h/cpp       # 看板列组件
│           ├── KanbanCard.h/cpp         # 看板卡片（支持拖拽）
│           ├── StatusBadge.h/cpp        # 状态标签
│           ├── ConversationThread.h/cpp # 对话线程组件（含 @ 支持）
│           └── FileTree.h/cpp           # 文件树组件
│
└── volt-ui/                            # git submodule
```

---

## 4. 数据模型与 Markdown 格式

### 4.1 projects.md — 项目索引

```markdown
# Projects

- [my-app] /home/user/projects/my-app — active
- [website] /home/user/projects/website — active
- [libcore] /home/user/projects/libcore — archived
```

**字段映射：**

| Markdown | Model 字段 | 说明 |
|----------|-----------|------|
| `[name] path` | name, repo | 项目名 + 关联仓库路径 |
| `— status` | status | active / archived |

### 4.2 config.md — 全局配置

```markdown
# Config

**default_agent**: opencode
**auto_save_interval**: 30
**scheduler_enabled**: true
**scheduler_check_interval**: 30
```

### 4.3 agents.md — Agent 定义

```markdown
# Agents

## opencode

- **name**: OpenCode
- **description**: 通用 AI 编码助手，适合日常编码
- **command**: opencode
- **args**:
- **model**: claude-sonnet-4
- **timeout**: 300
- **auto_approve**: low
- **enabled**: true
- **tags**: coding, review, refactor

### instructions

你是一个专业的软件工程师助手。请遵循：
1. 先理解项目结构再修改
2. 生成完整可运行的代码
3. 添加必要的错误处理
4. 遵循项目的代码风格和约定

### env

OPENCODE_MODEL=claude-sonnet-4
OPENCODE_TEMPERATURE=0.2

### skills

- code_generation
- code_review

---

## claude

- **name**: Claude
- **description**: 通用 AI，擅长架构设计和复杂推理
- **command**: claude
- **args**: -p
- **model**: claude-sonnet-4-20250506
- **timeout**: 600
- **auto_approve**: never
- **enabled**: true
- **tags**: architecture, planning, review

### instructions

你是资深软件架构师。关注：
1. 系统设计和架构决策
2. 模块间接口定义
3. 技术选型权衡
4. 可扩展性和可维护性

### env

ANTHROPIC_API_KEY=********

### skills

- architecture_design
- code_review
- technical_debt_analysis
```

**Agent 字段完整说明：**

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `name` | string | yes | 显示名称 |
| `description` | string | no | 一句话描述用途 |
| `command` | string | yes | CLI 可执行文件路径或名称 |
| `args` | string | no | 默认命令行参数 |
| `model` | string | no | 使用的模型标识 |
| `timeout` | int | no | 最大执行时间（秒），默认 300 |
| `auto_approve` | enum | no | 自动审批阈值: `low` / `medium` / `high` / `never`，默认 `never` |
| `enabled` | bool | no | 是否启用，默认 `true` |
| `tags` | list | no | 标签分类 |
| `instructions` | text | no | 系统指令/人格设定 |
| `env` | kv | no | 环境变量键值对 |
| `skills` | list | no | 引用的 Skill ID 列表 |

### 4.4 skills.md — Skill 定义

```markdown
# Skills

## code_generation

- **description**: 根据需求生成高质量代码

### instructions

根据需求生成代码：
- 遵循 SOLID 原则
- 包含类型定义和错误处理
- 添加必要的测试

---

## code_review

- **description**: 审查代码质量和安全性

### instructions

审查代码，关注：
1. 安全漏洞（注入、XSS、认证）
2. 性能瓶颈
3. 代码异味
4. 测试覆盖

---

## architecture_design

- **description**: 系统架构设计

### instructions

负责架构设计：
1. 明确模块边界和接口契约
2. 考虑扩展性和可维护性
3. 文档化架构决策记录（ADR）

---

## technical_debt_analysis

- **description**: 技术债务分析

### instructions

分析代码库中的技术债务：
1. 识别过时的依赖和弃用 API
2. 标记需要重构的模块
3. 评估修复优先级和工作量
```

### 4.5 project.md — 项目信息

```markdown
# Project: my-app

**repo**: /home/user/projects/my-app
**created**: 2026-05-06
**status**: active

## Tasks

- [UX-001] 实现用户登录功能 — active
- [UX-002] 添加仪表盘页面 — planning
- [UX-003] 每日依赖更新检查 — active
```

**字段说明：**

| 字段 | 必需 | 说明 |
|------|------|------|
| `name` | yes | 从 `# Project: <name>` 解析 |
| `repo` | yes | 关联的本地代码仓库路径 |
| `created` | yes | 创建时间 |
| `status` | yes | active / archived |
| `Tasks` | auto | 从 `tasks/` 目录自动扫描生成 |

### 4.6 Task 文件 — 核心文件格式

位于 `.codeyoyo/projects/<name>/tasks/<id>-<title>.md`。

```markdown
# UX-001: 实现用户登录功能

**status**: active
**priority**: high
**created**: 2026-05-06
**updated**: 2026-05-06

用户需要通过邮箱和密码登录，支持 JWT Token 认证。

---

## Subtasks

### ST-001: 创建 users 表

- **status**: completed
- **assignee**: opencode
- **risk**: low
- **approval**: auto
- **execution**: serial

创建数据库 users 表，包含 id, email, password_hash, created_at 字段。

> **[opencode]** (2026-05-06 14:20)
> 开始执行。
>
> #### 执行日志
>
> ```
> Created src/db/migrations/001_create_users.sql
> ```
>
> **[opencode]** (2026-05-06 14:21)
> ✅ 已完成，exit:0

---

### ST-002: 实现 JWT 认证逻辑

- **status**: in_progress
- **assignee**: claude
- **risk**: medium
- **approval**: confirm
- **execution**: serial
- **depends**: ST-001

实现登录 API 端点，JWT Token 签发和验证。

> **[claude]** (2026-05-06 14:30)
> JWT 密钥存哪里？环境变量还是配置文件？
>
> **[user]** (2026-05-06 14:31)
> 放环境变量，参考 .env.example
>
> **[claude]** (2026-05-06 14:32)
> 明白，开始实现。
>
> #### 执行日志
>
> ```
> Creating src/auth/jwt.ts...
> Writing login endpoint...
> ```
>
> **[user]** (2026-05-06 14:35)
> @opencode 审查一下 claude 生成的 JWT 代码，看有没有安全问题
>
> **[opencode]** (2026-05-06 14:36)
> 收到，开始审查。
> 发现一处潜在问题：Token 过期时间未验证...
```

### 4.7 带定时任务的 Task 文件

```markdown
# UX-003: 每日依赖更新检查

**status**: active
**priority**: low
**created**: 2026-05-06

**schedule**: 0 9 * * *
**timezone**: Asia/Shanghai
**next_run**: 2026-05-07 09:00
**last_run**: 2026-05-06 09:00
**assignee**: codex
**auto_approve**: low

每天早上 9 点检查项目依赖更新。

---

## Subtasks

### ST-001: 检查 npm 依赖

- **status**: pending
- **assignee**: codex
- **risk**: low
- **approval**: auto

运行 npm outdated，列出所有可更新的包。

### ST-002: 生成更新报告

- **status**: pending
- **assignee**: codex
- **risk**: low
- **approval**: auto

分析 breaking changes，生成更新建议报告。
```

**Task/Subtask 字段说明：**

**Task 字段：**

| 字段 | 必需 | 说明 |
|------|------|------|
| `ID: Title` | yes | 从 `# ID: Title` 解析 |
| `status` | yes | planning / active / completed / cancelled |
| `priority` | no | low / medium / high |
| `created` | yes | 创建时间 |
| `updated` | no | 最后更新时间 |
| `schedule` | no | Cron 表达式，如 `0 9 * * *` |
| `timezone` | no | 时区，默认系统时区 |
| `next_run` | no | 下次执行时间（自动计算） |
| `last_run` | no | 上次执行时间（自动更新） |
| `assignee` | no | 默认分配的 Agent ID |
| `auto_approve` | no | 覆盖默认审批策略 |

**Subtask 字段：**

| 字段 | 必需 | 说明 |
|------|------|------|
| `ID: Title` | yes | 从 `### ID: Title` 解析 |
| `status` | yes | pending / in_progress / review / completed / failed / cancelled |
| `assignee` | no | 分配的 Agent ID |
| `risk` | no | low / medium / high |
| `approval` | no | auto / confirm / agent_decides |
| `execution` | no | parallel / serial，执行模式 |
| `depends` | no | 前置依赖的 Subtask ID 列表，逗号分隔 |
| `result` | no | 执行结果摘要，自动更新 |

### 4.8 AUDIT.md — 审计日志

```markdown
# Audit Log

## 2026-05-06 14:20
- **action**: subtask_execute
- **actor**: user
- **detail**: ST-001 → opencode

## 2026-05-06 14:21
- **action**: subtask_complete
- **actor**: opencode
- **detail**: ST-001 completed (exit:0, 12s)

## 2026-05-06 14:35
- **action**: subtask_reply
- **actor**: user
- **detail**: @opencode 审查 ST-002

## 2026-05-06 14:36
- **action**: subtask_execute
- **actor**: opencode
- **detail**: ST-002 code review started
```

**审计日志动作类型：**

| action | 说明 |
|--------|------|
| project_create | 创建项目 |
| task_create | 创建 Task |
| task_update | 更新 Task |
| subtask_execute | 分配并执行 Subtask |
| subtask_complete | Subtask 完成 |
| subtask_fail | Subtask 失败 |
| subtask_reply | 用户/Agent 回复 |
| approval_confirm | 用户审批通过 |
| approval_reject | 用户审批拒绝 |
| schedule_trigger | 定时任务触发 |

---

## 5. Markdown 解析状态机

解析器是一个逐行状态机，按 heading 层级确定上下文。

### 5.1 解析规则

```
行类型判定（按优先级）：

1. "# Title"              → level-1 heading → Task 上下文
2. "## Subtasks"          → level-2 heading → Subtask 列表起始
3. "## text"              → level-2 heading → 普通 section
4. "### ST-N: Title"      → level-3 heading → Subtask 上下文
5. "#### 执行日志"        → level-4 heading → 原始日志区块
6. "#### text"            → level-4 heading → 子 section
7. "**key**: value"       → 元数据键值对（当前上下文）
8. "- **key**: value"     → 列表形式的元数据键值对
9. "> **[sender]** (ts)"  → 对话消息起始
10. "> text"              → 对话消息正文（追加到上一条）
11. "```"                 → 代码块（切换 in_code_block 状态）
12. "---"                 → 分隔符
13. 其他文本              → 当前上下文的描述正文
```

### 5.2 状态机实现

```
enum ParseState {
    TopLevel,          // 文件顶级
    InTaskMeta,        // 在 Task 元数据区（heading 1 下）
    InSubtaskList,     // 在 ## Subtasks 下
    InSubtaskMeta,     // 在具体 Subtask 元数据区（### 下）
    InConversation,    // 在对话 blockquote 中
    InExecLog,         // 在 #### 执行日志 下
    InCodeBlock,       // 在 ``` 代码块中
    InSection          // 在普通 section 中
}
```

### 5.3 合并策略

Agent/Skill 支持全局 + 项目级合并：

```
全局 Agent "opencode":
  name: OpenCode
  command: opencode
  env: OPENCODE_MODEL=claude-sonnet-4

项目 agents.md 同名 Agent：
  env: OPENCODE_MODEL=claude-haiku-3.5

合并结果：
  name: OpenCode              ← 继承全局
  command: opencode            ← 继承全局
  env: OPENCODE_MODEL=claude-haiku-3.5  ← 项目覆盖
  instructions: ...            ← 继承全局
```

合并规则：**浅合并**。标量字段以项目级为准，列表和字典以项目级为准完全替换。

---

## 6. 核心模块设计

### 6.1 ProjectManager

```
class ProjectManager {
    // 索引
    vector<Project> ListProjects();
    Project* GetActiveProject();
    
    // CRUD
    Project CreateProject(string name, string repo);
    void RemoveProject(string id);
    void SwitchProject(string id);
    
    // 存储
    void SaveProject(const Project& p);
    void LoadProjects();     // 从 projects.md
    void LoadProjectTasks(); // 从 tasks/ 目录
}
```

### 6.2 TaskManager

```
class TaskManager {
    // CRUD
    Task CreateTask(string project_id, string title, string description);
    void UpdateTask(const Task& task);
    void DeleteTask(string task_id);
    
    // Subtask
    Subtask CreateSubtask(string task_id, string title);
    void UpdateSubtaskStatus(string subtask_id, SubtaskStatus s);
    
    // 状态转换约束
    // pending → in_progress → review → completed
    //     ↓          ↓
    //  cancelled   failed
    
    // 查询
    Task* GetTask(string id);
    vector<Subtask> GetSubtasksByStatus(TaskStatus s);
}
```

### 6.3 AgentManager

```
class AgentManager {
    void LoadGlobalAgents();   // 从 ~/.codeyoyo/agents.md
    void LoadProjectAgents();  // 从 project/agents.md (叠加)
    Agent* GetAgent(string id);
    Agent GetMergedAgent(string id, string project_id); // 合并
    vector<Agent> ListAvailable();
}
```

### 6.4 SkillManager

```
class SkillManager {
    void LoadSkills();         // 从 ~/.codeyoyo/skills.md
    Skill* GetSkill(string id);
    string BuildPrompt(string agent_id, string skill_id); // 合并 instructions
}
```

### 6.5 Executor

```
class Executor {
    // 执行一个 Subtask
    ExecutionResult Execute(Subtask& sub, const Agent& agent, const Project& project);
    
    // 并行执行
    void ExecuteParallel(vector<Subtask*> subs);
    
    // 串行执行（队列）
    void ExecuteSerial(vector<Subtask*> subs);
    
    // 取消
    void Cancel(string subtask_id);
}
```

### 6.6 ProcessManager

```
class ProcessManager {
    // 启动子进程
    ProcessHandle Spawn(string command, vector<string> args,
                        string cwd, map<string,string> env);
    
    // 交互（写入 stdin + 读取 stdout/stderr）
    void Write(std::string data);
    void OnOutput(std::function<void(string)> callback);  // 流式回调
    
    // 控制
    void Terminate(string id);
    int Wait(string id, int timeout_ms);
}
```

**Windows PTY 实现要点：**

使用 Windows ConPTY API（`CreatePseudoConsole`），而非简单的匿名管道。因为大多数 AI CLI 工具（opencode、claude 等）需要 TTY 才能正常运行。

```
CreatePseudoConsole → 关联到子进程的 STARTUPINFOEX
                     → 读取输出到 UI 控制台
                     → 写入输入（支持交互式）
```

### 6.7 ApprovalGate

```
class ApprovalGate {
    // 评估风险
    RiskLevel AssessRisk(const Subtask& sub);
    
    // 判断是否需要审批
    bool NeedsApproval(const Subtask& sub, const Agent& agent);
    
    // 审批操作
    void Approve(string subtask_id);
    void Reject(string subtask_id, string reason);
    
    // 回调
    void OnPendingReview(function<void(Subtask&)> callback);
}
```

**风险评级规则：**

| 操作 | 默认风险 |
|------|---------|
| 读取文件/搜索代码 | low |
| 新建文件 | low |
| 修改文件 | medium |
| 删除文件 | high |
| git push | high |
| 安装依赖 | high |
| 执行 shell 命令 | high |

### 6.8 Scheduler

```
class Scheduler {
    void Start();          // 启动轮询线程
    void Stop();           // 停止
    void SetInterval(int seconds);  // 轮询间隔
    
    // 检查并触发
    void CheckAndTrigger();
    
    // 添加/移除调度
    void AddSchedule(string task_id, string cron_expr);
    void RemoveSchedule(string task_id);
    
    // Cron 解析
    time_t CalculateNextRun(string cron_expr);
    bool IsDue(string cron_expr, time_t last_run);
}
```

**调度引擎工作流：**

```
每 30s 轮询：
  └── 遍历所有 active 项目
       └── 遍历每个项目的 active Task
            └── 有 schedule 字段？
                 ├── 计算 next_run
                 ├── next_run ≤ now？
                 │    ├── 触发执行（同用户手动分配）
                 │    ├── 更新 last_run = now
                 │    ├── 计算下一次 next_run
                 │    └── 写入 AUDIT.md
                 └── 否 → 跳过
```

---

## 7. UI 设计

### 7.1 整体布局（ImGui Docking）

```
┌──────────────────────────────────────────────────────────┐
│  Topbar: [CodeYoYo]  [Project: my-app ▼]  [⚙ Settings]  │
├────────────┬──────────────────────────┬──────────────────┤
│ 左侧面板     │    中央主区                │ 右侧面板          │
│ (可折叠)    │                          │ (可折叠)          │
│            │  ┌──────────────────────┐ │                  │
│ 📁 Projects │  │  [看板] [树形] 视图切换 │ │ 📋 Task Detail  │
│ ─────────  │  │                      │ │                  │
│ ▸ my-app   │  │ ┌────┐┌────┐┌────┐  │ │ # UX-001: 实现   │
│ ▸ website  │  │ │待处 ││进行││完成│  │ │ 用户登录功能      │
│ ▸ libcore  │  │ │理   ││中  ││   │  │ │                  │
│            │  │ ├────┤├────┤├────┤  │ │ status: active   │
│ + New      │  │ │ST-1││ST-2││ST-3│  │ │ priority: high   │
│            │  │ │    ││    ││    │  │ │                  │
│            │  │ │    ││    ││    │  │ │ ─── Subtasks ─── │
│            │  │ └────┘└────┘└────┘  │ │                  │
│            │  └──────────────────────┘ │ ST-001 创建users │
│            │                          │ [completed]       │
│            │                          │                  │
│            │                          │ ST-002 JWT认证   │
│            │                          │ [in_progress] ◀  │
│            │                          │ > claude: 密钥    │
│            │                          │ > user: 放环境    │
│            │                          │ > claude: 开始    │
│            │                          │ [输入回复...]      │
│            │                          │ [@agent] [发送]   │
├────────────┴──────────────────────────┴──────────────────┤
│  🔧 Agent Console (底部可伸缩面板)                        │
│  [Agent: claude ● 运行中] [Task: UX-001/ST-002]         │
│  ┌──────────────────────────────────────────────────────┐ │
│  │ $ claude -p "实现JWT认证逻辑"                          │ │
│  │ > Creating src/auth/jwt.ts...                         │ │
│  │ > Writing login endpoint...                            │ │
│  │ > ✅ Done (2 files modified)  [审查 Diff ▸]          │ │
│  └──────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────┘
```

### 7.2 面板说明

| 面板 | 位置 | 功能 |
|------|------|------|
| ProjectPanel | 左侧 | 项目列表，新建/切换/删除项目 |
| TaskBoard | 中央 | 看板视图，按状态列展示 Subtask 卡片 |
| TaskTree | 中央 | 树形列表视图，按 Task → Subtask 层级展示 |
| TaskDetail | 右侧 | 选中 Task 的完整信息 + Subtask 对话线程 |
| AgentConsole | 底部 | Agent 执行时的实时流式输出 |
| DiffReview | 弹窗/右侧 | 展示文件变更 diff，逐块确认/拒绝 |
| SchedulePanel | 弹窗/面板 | 定时任务管理 |
| SettingsPanel | 弹窗 | Agent/Skill 配置管理 |

### 7.3 看板视图

看板按 Subtask 的状态分列：

```
┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│  待处理      │ │  执行中      │ │  待审查      │ │  已完成      │
│  (pending)  │ │ (in_progress)│ │  (review)   │ │ (completed) │
├─────────────┤ ├─────────────┤ ├─────────────┤ ├─────────────┤
│ ST-003      │ │ ST-002      │ │             │ │ ST-001      │
│ 写登录UI    │ │ JWT认证     │ │             │ │ 创建users表 │
│ ─────────   │ │ ─────────   │ │             │ │ ─────────   │
│ low         │ │ medium      │ │             │ │ low         │
│ opencode    │ │ claude      │ │             │ │ opencode    │
│             │ │             │ │             │ │ ✅ exit:0   │
└─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘
```

**卡片支持拖拽：**
- 拖到不同列 → 改变 status
- 拖到 Agent 名上 → 分配/切换 Agent
- 右键菜单 → 执行、取消、查看详情

### 7.4 对话区（@agent 支持）

对话区位于 TaskDetail 面板中，每个 Subtask 下方：

```
─────────────────────────────────────
ST-002: 实现 JWT 认证逻辑
─────────────────────────────────────
[in_progress] [claude] [medium]
─────────────────────────────────────

> **[claude]** (14:30)
> JWT 密钥存哪里？环境变量还是配置文件？

> **[user]** (14:31)
> 放环境变量，参考 .env.example

> **[claude]** (14:32)
> 明白，开始实现。

> **[user]** (14:35)
> @opencode 审查一下 claude 生成的 JWT 代码

> **[opencode]** (14:36)
> 收到，开始审查...
─────────────────────────────────────
[ 输入回复...              ] [@ ▼] [发送]
─────────────────────────────────────
```

**@ 自动补全：** 输入 `@` 时弹出 Agent 列表，支持模糊搜索。

---

## 8. 核心工作流

### 8.1 创建和执行 Task

```
用户创建 Task "实现用户登录功能"
  │
  ▼
TaskManager.CreateTask() → 写入 .md 文件
  │
  ▼
用户添加 Subtask（或 AI 辅助拆解）
  │
  ▼
用户将 Subtask 分配给 Agent
  │
  ▼
Executor.Execute(subtask, agent, project)
  │
  ├── ProcessManager.Spawn(command, args, cwd, env)
  │     └── 子进程启动 → PTY 交互
  │
  ├── 流式输出 → AgentConsole 实时显示
  │
  ├── 执行完成
  │     ├── ApprovalGate.NeedsApproval?
  │     │    ├── 否 → auto-approve → 更新 status = completed
  │     │    └── 是 → status = review → 等待用户审查
  │     │
  │     └── 更新 .md 文件 + AUDIT.md
  │
  ▼
用户审查 Diff → Approve / Reject
  │
  ▼
更新状态 → 看板自动刷新
```

### 8.2 @agent 协作流程

```
用户在 Subtask 对话中回复 "@opencode 审查代码"
  │
  ▼
系统检测到 @opencode
  │
  ├── 将回复写入 .md 文件
  │
  └── 查找 opencode Agent
       ├── enabled？否 → 提示 Agent 未启用
       │
       └── 是 → 自动触发执行
            ├── 上下文 = 该 Subtask 完整对话历史
            ├── Prompt = 将用户回复发给 Agent
            ├── Agent 执行
            └── 结果追加到同一 Subtask 对话区
```

### 8.3 定时任务触发流程

```
Scheduler 轮询线程（每 30s）
  │
  ▼
遍历所有 active 项目的 active Task
  │
  ▼
找到 UX-003: schedule = "0 9 * * *", next_run = "2026-05-07 09:00"
  │
  ▼
当前时间 ≥ next_run？
  │
  ├── 否 → 跳过
  │
  └── 是 → 触发执行
       ├── 记录到 AUDIT.md (schedule_trigger)
       ├── 更新 last_run = now
       ├── 计算下一次 next_run
       ├── 写入 Task .md 文件
       └── 执行 Subtask（同手动执行流程）
```

---

## 9. 关键实现技术点

### 9.1 ImGui Docking 集成

修改 `VoltApp.cpp` 的 `InitImGui()`，增加 Docking 支持：

```cpp
io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // 可选：浮动窗口
```

在 `OnRender()` 中：

```cpp
// 创建 Dockspace
ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

// 各面板通过 Begin() 自动停靠
ProjectPanel::Render();
TaskBoardPanel::Render();
TaskDetailPanel::Render();
AgentConsolePanel::Render();
```

### 9.2 Windows ConPTY 封装

Windows 上启动交互式子进程的核心步骤：

```cpp
// 1. 创建伪控制台
HPCON hpc;
HRESULT hr = CreatePseudoConsole(
    size,           // 缓冲区大小
    hInput,         // 输入管道读取端
    hOutput,        // 输出管道写入端
    0, &hpc
);

// 2. 配置子进程启动信息
STARTUPINFOEX si;
PrepareStartupInfo(&si, hpc);

// 3. 创建子进程
CreateProcess(
    nullptr, commandLine,
    nullptr, nullptr, FALSE,
    EXTENDED_STARTUPINFO_PRESENT,
    envBlock, cwd,
    &si.StartupInfo, &pi
);

// 4. 读取输出（在 worker 线程）
HANDLE hOutputRead = /* 管道的读取端 */;
char buf[4096];
DWORD read;
while (ReadFile(hOutputRead, buf, sizeof(buf), &read, nullptr)) {
    // 将输出发送到 UI 线程
    onOutput(string(buf, read));
}
```

### 9.3 线程模型

```
主线程 (UI)                    Worker 线程 (Agent 执行)
─────────────────              ─────────────────
Render() 循环                   子进程运行
  │                               │
  ├── UI 事件处理                  ├── 读取 stdout
  ├── 面板渲染                     ├── 写入 stdin
  └── 检查消息队列                 └── 退出 →
       │                                    │
       └──← 消息 (输出/完成/错误) ←─────────┘
```

UI 更新通过**线程安全队列**（`moodycamel::ConcurrentQueue` 或 `std::queue + mutex`）从 worker 线程传递到主线程。

### 9.4 Cron 解析

使用现有的 C/C++ 库，如 [ccronexpr](https://github.com/staticlibs/ccronexpr)（单个 .c/.h 文件，无依赖）：

```cpp
#include "ccronexpr.h"

// 解析 cron 表达式
cron_expr expr;
const char* err = NULL;
cron_parse_expr("0 9 * * *", &expr, &err);

// 计算下次执行时间
time_t base = time(nullptr);
time_t next = cron_next(&expr, base);

// 判断是否到期
bool IsDue(time_t next_run) {
    return time(nullptr) >= next_run;
}
```

---

## 10. 第三方依赖

| 依赖 | 用途 | 引入方式 |
|------|------|---------|
| `nlohmann/json` | JSON 辅助（用于配置等临时场景） | Header-only 或 CPM |
| `ccronexpr` | Cron 表达式解析 | 单 .c/.h 文件 |
| `libgit2` | Git 集成（可选） | 系统包或 submodule |
| `moodycamel::ConcurrentQueue` | 线程安全消息队列 | Header-only |

**现有依赖（volt-ui 已有）：**

| 依赖 | 用途 |
|------|------|
| SDL3 | 窗口、输入、渲染 |
| Dear ImGui | GUI |
| LeanCLR | C# 脚本（可选） |
| log.c | 日志 |

---

## 11. 实施路线

### Phase 1 — 基础框架

目标：可运行的应用，能创建项目、看板展示 Task

- [x] 现有：SDL3 + ImGui 窗口 + Topbar
- [ ] 启用 ImGui Docking
- [ ] 实现 MarkdownParser（读取 project.md + Task .md）
- [ ] 实现 MarkdownWriter（写入/更新 .md 文件）
- [ ] 实现 ProjectManager（CRUD + 索引）
- [ ] 实现 TaskManager（CRUD + 状态管理）
- [ ] 实现 ProjectPanel（左侧列表）
- [ ] 实现 TaskBoardPanel（静态看板，不可拖拽）
- [ ] 实现 TaskDetailPanel（基本信息展示）
- [ ] 基础 Theme 样式

### Phase 2 — Agent 执行

目标：能通过 Agent 执行 Subtask

- [ ] 实现 AgentManager + 项目级合并
- [ ] 实现 SkillManager
- [ ] 实现 ProcessManager（Windows ConPTY 封装）
- [ ] 实现 Executor（串行 + 并行调度）
- [ ] 实现 AgentConsolePanel（流式输出）
- [ ] 实现对话回复机制（blockquote 解析/写入）
- [ ] 实现 @agent 自动补全 + 触发执行
- [ ] SettingsPanel（Agent/Skill 配置 UI）

### Phase 3 — 完善

目标：审批、定时任务、视图切换

- [ ] 实现 ApprovalGate + 风险评级
- [ ] 实现 DiffReviewPanel
- [ ] 实现 Scheduler（Cron 轮询引擎）
- [ ] 实现 SchedulePanel
- [ ] 看板卡片拖拽（改变状态 + 分配 Agent）
- [ ] TaskTreePanel（树形列表视图）
- [ ] AI 辅助 Task 拆解
- [ ] 审计日志

---

## 12. 设计决策记录

| 决策 | 选项 | 最终选择 | 理由 |
|------|------|---------|------|
| 项目存储位置 | 集中式 / 仓库内 | 集中式 `~/.codeyoyo/` | 解耦项目管理与代码仓库 |
| Task 文件格式 | 单文件 / 每 Task 一文件 | 每 Task 一文件 | 独立编辑、版本控制友好 |
| 数据格式 | JSON / YAML / Markdown | 纯 Markdown + 列表 | 人类可读可编辑，无 frontmatter |
| 对话格式 | Blockquote / Heading + 列表 | Blockquote `> **[s]** (t)` | 干净，与 Markdown 兼容 |
| 调度表达式 | Cron / 自然语言 | Cron `0 9 * * *` | 标准精确 |
| Agent 覆盖 | 允许 / 全局唯一 | 允许项目级覆盖合并 | 灵活适配项目需求 |
| @agent 行为 | 同 Subtask / 新建 Subtask | 追加到同一 Subtask | 保持上下文集中 |
| 子进程交互 | Pipe / ConPTY | ConPTY (Windows) | CLI 工具需要 TTY |
| 线程模型 | 同步 / 异步 + 消息队列 | 异步 + 消息队列 | 不阻塞 UI 渲染 |
