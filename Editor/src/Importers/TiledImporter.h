#pragma once

#include <filesystem>
#include <string>
#include <vector>

class Entity;

class TiledImporter
{
public:
	// Both return why the import failed, or an empty string if it worked
	// Adds the roots it creates in the open scene to createdRoots, if given
	static std::string ImportTilemap(const std::filesystem::path& filepath, const std::filesystem::path& destination,
		std::vector<Entity>* createdRoots = nullptr);
	static std::string ImportTileset(const std::filesystem::path& filepath, const std::filesystem::path& destination);
};
