#include <iostream>

#include "Logger.hpp"

int main(int, char**)
{
	atcp::Logger::Init("App");
	LOG_INFO("Hello World!");
	return EXIT_SUCCESS;
}