#include "Attercop.hpp"

int main(int argc, char* argv[])
{
	atcp::Application app;
	if (app.Init(argc, argv) != 0)
		EXIT_FAILURE;

	app.Run();

	return EXIT_SUCCESS;
}
