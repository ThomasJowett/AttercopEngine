#pragma once

#include <filesystem>
#include <string>

class TiledImporter
{
public:
	// Both return why the import failed, or an empty string if it worked
	static std::string ImportTilemap(const std::filesystem::path& filepath, const std::filesystem::path& destination);
	static std::string ImportTileset(const std::filesystem::path& filepath, const std::filesystem::path& destination);
};
