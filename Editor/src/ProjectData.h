#pragma once

#include <string>

struct ProjectData
{
	std::string defaultScene;

	std::string description;

	// Project-relative path to a PNG
	std::string icon;

	uint32_t spriteAtlasPageSize = 2048;
};
