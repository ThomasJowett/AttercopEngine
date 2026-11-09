#pragma once

#include <filesystem>

namespace WindowsIcon
{
	// Adds a PNG to an .exe as its icon resource, before anything is appended to the file
	bool AddToExecutable(const std::filesystem::path& executablePath, const std::filesystem::path& pngPath);
}
