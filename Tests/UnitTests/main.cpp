#define DOCTEST_CONFIG_IMPLEMENT
#include "vendor/doctest.h"

#include "Logging/Logger.h"

#include <filesystem>

int main(int argc, char** argv)
{
	// Engine code logs warnings, which needs a logger
	Logger::Init(std::filesystem::temp_directory_path() / "AttercopUnitTests.log");

	doctest::Context context(argc, argv);
	return context.run();
}
