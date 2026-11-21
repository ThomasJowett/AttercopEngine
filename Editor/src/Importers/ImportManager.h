#pragma once

#include <filesystem>
#include <string>
#include <vector>

class Entity;

class ImportManager
{
public:
	// Returns why the import failed, or an empty string if it worked; a .tmx adds the roots it creates in the open scene to createdRoots
	static std::string ImportAsset(const std::filesystem::path& asset, const std::filesystem::path& destination,
		std::vector<Entity>* createdRoots = nullptr);

	// Returns why any imports failed, one line per file, or an empty string if they all worked
	static std::string ImportMultiAssets(const std::vector<std::filesystem::path>& assets, const std::filesystem::path& destination);
	static std::string ImportMultiAssets(const std::vector<std::wstring>& assets, const std::filesystem::path& destination);
};
