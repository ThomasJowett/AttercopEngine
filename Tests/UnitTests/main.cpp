#define DOCTEST_CONFIG_IMPLEMENT
#include "vendor/doctest.h"

#include "Core/Application.h"
#include "Logging/Logger.h"

#include <filesystem>

namespace
{
	// Assets and fonts load relative to the open project, which needs an Application
	struct TestApplication : Application {};
}

int main(int argc, char** argv)
{
	// Engine code logs warnings, which needs a logger
	Logger::Init(std::filesystem::temp_directory_path() / "AttercopUnitTests.log");

	// Never destroyed, its destructor would save the user's settings
	new TestApplication();

	doctest::Context context(argc, argv);
	return context.run();
}
