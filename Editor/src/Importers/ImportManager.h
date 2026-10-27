#pragma once

#include <filesystem>
#include <string>
#include <vector>

class ImportManager
{
public:
	// Returns why the import failed, or an empty string if it worked
	static std::string ImportAsset(const std::filesystem::path& asset, const std::filesystem::path& destination);

	// Returns why any imports failed, one line per file, or an empty string if they all worked
	static std::string ImportMultiAssets(const std::vector<std::filesystem::path>& assets, const std::filesystem::path& destination);
	static std::string ImportMultiAssets(const std::vector<std::wstring>& assets, const std::filesystem::path& destination);
};
