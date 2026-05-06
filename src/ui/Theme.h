#pragma once

#include <imgui.h>

namespace Theme {

void ApplyDark();
ImU32 ColorForStatus(const std::string& status);
ImU32 ColorForRisk(const std::string& risk);
const char* StatusIcon(const std::string& status);

}
