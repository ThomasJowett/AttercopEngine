#pragma once

#include "imgui.h"

#include <string>

// A menu item's shortcut text: Cmd and macKeys on macOS (or in a browser on one), Ctrl and keys elsewhere
inline std::string ShortcutLabel(const char* keys, const char* macKeys = nullptr)
{
	if (ImGui::GetIO().ConfigMacOSXBehaviors)
		return std::string("Cmd + ") + (macKeys ? macKeys : keys);
	return std::string("Ctrl + ") + keys;
}
