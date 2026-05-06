#include "Task.h"

Subtask* Task::FindSubtask(const std::string& sid) {
    for (auto& s : subtasks) {
        if (s.id == sid) return &s;
    }
    return nullptr;
}

SubtaskStatus Task::ParseStatus(const std::string& s) const {
    if (s == "pending") return SubtaskStatus::Pending;
    if (s == "in_progress") return SubtaskStatus::InProgress;
    if (s == "review") return SubtaskStatus::Review;
    if (s == "completed") return SubtaskStatus::Completed;
    if (s == "failed") return SubtaskStatus::Failed;
    if (s == "cancelled") return SubtaskStatus::Cancelled;
    return SubtaskStatus::Pending;
}

std::string Task::StatusString(SubtaskStatus s) const {
    switch (s) {
        case SubtaskStatus::Pending: return "pending";
        case SubtaskStatus::InProgress: return "in_progress";
        case SubtaskStatus::Review: return "review";
        case SubtaskStatus::Completed: return "completed";
        case SubtaskStatus::Failed: return "failed";
        case SubtaskStatus::Cancelled: return "cancelled";
    }
    return "pending";
}
