#pragma once

#include <string>
#include <vector>
#include <imgui.h>
#include "models/Task.h"

namespace ConversationThread {

void Render(const std::vector<ConversationMsg>& messages);

} // namespace ConversationThread