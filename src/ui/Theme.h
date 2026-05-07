#pragma once

#include <imgui.h>
#include <string>

namespace Theme {

void ApplyDark();
void LoadFonts();
ImFont* GetDefaultFont();
ImFont* GetIconFont();
ImU32 ColorForStatus(const std::string& status);
ImU32 ColorForRisk(const std::string& risk);
const char* StatusIcon(const std::string& status);

}
