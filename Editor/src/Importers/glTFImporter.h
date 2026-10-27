#pragma once

#include <filesystem>
#include <string>

class glTFImporter
{
public:
	// Returns why the import failed, or an empty string if it worked
	static std::string ImportAssets(const std::filesystem::path& filepath, const std::filesystem::path& destination);
};
