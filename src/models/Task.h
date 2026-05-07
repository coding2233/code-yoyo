#pragma once

#include <string>
#include <vector>
#include <map>

enum class SubtaskStatus {
    Pending,
    InProgress,
    Review,
    Completed,
    Failed,
    Cancelled
};

enum class RiskLevel {
    Low,
    Medium,
    High
};

enum class ApprovalMode {
    Auto,
    Confirm,
    AgentDecides
};

enum class ExecMode {
    Parallel,
    Serial
};

struct ConversationMsg {
    std::string sender;
    std::string timestamp;
    std::string body;
    bool is_exec_log = false;
};

struct Subtask {
    std::string id;
    std::string title;
    std::string description;
    SubtaskStatus status = SubtaskStatus::Pending;
    std::string assignee;
    RiskLevel risk = RiskLevel::Low;
    ApprovalMode approval = ApprovalMode::Auto;
    ExecMode exec_mode = ExecMode::Serial;
    std::vector<std::string> depends;
    std::string result;
    std::vector<ConversationMsg> conversation;
};

struct Task {
    std::string id;
    std::string title;
    std::string description;
    std::string status = "active";
    std::string priority = "medium";
    std::string created;
    std::string updated;

    std::string schedule;
    std::string timezone;
    std::string next_run;
    std::string last_run;
    std::string assignee;
    std::string auto_approve;

    std::vector<Subtask> subtasks;

    Subtask* FindSubtask(const std::string& sid);
    const Subtask* FindSubtask(const std::string& sid) const;
    SubtaskStatus ParseStatus(const std::string& s) const;
    std::string StatusString(SubtaskStatus s) const;
};
