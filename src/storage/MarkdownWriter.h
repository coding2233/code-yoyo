#pragma once

#include <string>
#include "models/Task.h"

class MarkdownWriter {
public:
    // Update a subtask's status in a task file
    static std::string UpdateSubtaskStatus(const std::string& content,
                                            const std::string& subtask_id,
                                            const std::string& new_status,
                                            const std::string& result);

    // Add a conversation message to a subtask
    static std::string AddConversationMsg(const std::string& content,
                                           const std::string& subtask_id,
                                           const std::string& sender,
                                           const std::string& timestamp,
                                           const std::string& body,
                                           bool is_exec_log = false);

    // Update schedule fields
    static std::string UpdateSchedule(const std::string& content,
                                       const std::string& last_run,
                                       const std::string& next_run);

    // Add audit entry
    static std::string FormatAuditEntry(const std::string& timestamp,
                                         const std::string& action,
                                         const std::string& actor,
                                         const std::string& detail);
};
