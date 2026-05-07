#pragma once

#include <string>
#include <imgui.h>

namespace StatusBadge {

void Render(const std::string& status, const std::string& label = "");
void RenderRisk(const std::string& risk);
void RenderExecutionStatus(int status_code);

}