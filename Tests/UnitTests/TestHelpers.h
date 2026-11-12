#pragma once

#include <filesystem>

// A file in the repository, for tests that need a real asset
inline std::filesystem::path RepositoryPath(const std::filesystem::path& relativePath)
{
	return std::filesystem::path(ATTERCOP_SOURCE_DIR) / relativePath;
}
