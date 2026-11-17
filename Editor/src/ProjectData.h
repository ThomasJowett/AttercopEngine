#pragma once

#include <string>

#include "Migration/Migration.h"

struct ProjectData
{
	std::string defaultScene;

	std::string description;

	// Project-relative path to a PNG
	std::string icon;

	uint32_t spriteAtlasPageSize = 2048;

	// The FormatVersion on disk, which only Migration::UpgradeProject raises
	int formatVersion = Migration::CurrentVersion(Migration::FileType::Project);
};
