#pragma once

#include "Logging/Logger.h"

#include <spdlog/sinks/ostream_sink.h>

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>

// A file in the repository, for tests that need a real asset
inline std::filesystem::path RepositoryPath(const std::filesystem::path& relativePath)
{
	return std::filesystem::path(ATTERCOP_SOURCE_DIR) / relativePath;
}

// Collects the engine's warnings while it exists
struct WarningCapture
{
	std::ostringstream stream;
	std::shared_ptr<spdlog::sinks::ostream_sink_mt> sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);

	WarningCapture()
	{
		sink->set_level(spdlog::level::warn);
		Logger::GetEngineLogger()->sinks().push_back(sink);
	}
	~WarningCapture()
	{
		std::vector<spdlog::sink_ptr>& sinks = Logger::GetEngineLogger()->sinks();
		sinks.erase(std::remove(sinks.begin(), sinks.end(), sink), sinks.end());
	}
	std::string Text()
	{
		Logger::GetEngineLogger()->flush();
		return stream.str();
	}
};
