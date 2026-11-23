#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace FileUtils
{
	std::filesystem::path RelativePath(const std::filesystem::path& path, const std::filesystem::path& relative_to);

	// True for absolute paths from any platform, including "C:/..." on Linux and macOS
	bool IsAbsolutePath(const std::filesystem::path& path);

	// Makes an absolute path relative to the project if it is in the project, or if its tail
	// (e.g. "Scripts/Player.lua" from "F:/OldMachine/Project/Scripts/Player.lua") exists in the project
	std::filesystem::path ToProjectRelative(const std::filesystem::path& path, const std::filesystem::path& projectDirectory);

	// Normalises separators to '/' for path matching
	std::string NormalisePath(const std::filesystem::path& path);

	// The part of reference after path: empty for the path itself, "/rest" for something inside it, nullopt otherwise
	std::optional<std::string> MatchReference(const std::string& reference, const std::string& path);
}
