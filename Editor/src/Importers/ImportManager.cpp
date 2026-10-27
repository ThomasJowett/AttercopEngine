#include "ImportManager.h"

#include "glTFImporter.h"
#include "TiledImporter.h"

#include "Logging/Logger.h"

#include <algorithm>

std::string ImportManager::ImportAsset(const std::filesystem::path& asset, const std::filesystem::path& destination)
{
	std::string extString = asset.extension().string();

	std::transform(extString.begin(), extString.end(), extString.begin(), ::tolower);

	std::string error;
	if (extString == ".tmx")
	{
		error = TiledImporter::ImportTilemap(asset, destination);
	}
	else if (extString == ".tsx")
	{
		error = TiledImporter::ImportTileset(asset, destination);
	}
	else if (extString == ".gltf" || extString == ".glb")
	{
		error = glTFImporter::ImportAssets(asset, destination);
	}
	else
	{
		error = extString + " files can't be imported. Supported formats are glTF (.gltf, .glb) and Tiled (.tmx, .tsx).";
	}

	if (!error.empty())
	{
		error = asset.filename().string() + ": " + error;
		CLIENT_ERROR("Could not import {0}", error);
	}
	return error;
}

std::string ImportManager::ImportMultiAssets(const std::vector<std::filesystem::path>& assets, const std::filesystem::path& destination)
{
	std::string errors;
	for (const auto& asset : assets)
	{
		std::string error = ImportAsset(asset, destination);
		if (!error.empty())
			errors += (errors.empty() ? "" : "\n") + error;
	}
	return errors;
}

std::string ImportManager::ImportMultiAssets(const std::vector<std::wstring>& assets, const std::filesystem::path& destination)
{
	return ImportMultiAssets(std::vector<std::filesystem::path>(assets.begin(), assets.end()), destination);
}
