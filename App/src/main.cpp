#include <iostream>

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "Logger.hpp"
#include "Application.hpp"

int main(int argc, char* argv[])
{
	atcp::Logger::Init("App");
	atcp::Application app;
	if (app.Init(argc, argv) != 0)
		EXIT_FAILURE;

	app.Run();

	return EXIT_SUCCESS;
}