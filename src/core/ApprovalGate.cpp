#include "ApprovalGate.h"
#include <algorithm>

ApprovalGate::RiskLevel ApprovalGate::AssessRisk(const Subtask& sub) {
    return static_cast<RiskLevel>(sub.risk);
}

bool ApprovalGate::NeedsApproval(const Subtask& sub, const Agent& agent) {
    auto sub_risk = AssessRisk(sub);

    // Map agent auto_approve to a numeric threshold
    auto threshold = [](const std::string& s) -> int {
        if (s == "low") return 0;
        if (s == "medium") return 1;
        if (s == "high") return 2;
        return -1; // "never"
    };

    auto sub_risk_int = static_cast<int>(sub_risk);
    auto agent_threshold = threshold(agent.auto_approve);

    // If agent says "never" auto-approve, always require approval
    if (agent_threshold < 0) return true;

    // Needs approval if subtask risk exceeds agent's auto-approve threshold
    return sub_risk_int > agent_threshold;
}

const char* ApprovalGate::RiskToString(RiskLevel r) {
    switch (r) {
        case RiskLevel::Low: return "low";
        case RiskLevel::Medium: return "medium";
        case RiskLevel::High: return "high";
    }
    return "unknown";
}

const char* ApprovalGate::RiskDescription(const std::string& action) {
    auto lower = action;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("read") != std::string::npos || lower.find("search") != std::string::npos || lower.find("find") != std::string::npos) {
        return "low";
    }
    if (lower.find("create") != std::string::npos || lower.find("new") != std::string::npos || lower.find("add") != std::string::npos) {
        return "low";
    }
    if (lower.find("modify") != std::string::npos || lower.find("edit") != std::string::npos || lower.find("update") != std::string::npos || lower.find("change") != std::string::npos) {
        return "medium";
    }
    if (lower.find("delete") != std::string::npos || lower.find("remove") != std::string::npos || lower.find("push") != std::string::npos || lower.find("install") != std::string::npos || lower.find("shell") != std::string::npos || lower.find("exec") != std::string::npos) {
        return "high";
    }
    return "medium";
}