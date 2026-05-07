#pragma once

#include <string>
#include <functional>
#include "models/Task.h"
#include "models/Agent.h"

class ApprovalGate {
public:
    enum class RiskLevel {
        Low,
        Medium,
        High
    };

    static RiskLevel AssessRisk(const Subtask& sub);

    static bool NeedsApproval(const Subtask& sub, const Agent& agent);

    static const char* RiskToString(RiskLevel r);
    static const char* RiskDescription(const std::string& action);

    using ReviewCallback = std::function<void(const std::string& subtask_id)>;
    void SetOnPendingReview(ReviewCallback cb) { on_pending_review_ = std::move(cb); }

private:
    ReviewCallback on_pending_review_;
};