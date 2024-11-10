#include "Attercop.hpp"

void closeAfterDelay(int seconds, atcp::Application& app) {
	std::this_thread::sleep_for(std::chrono::seconds(seconds));
	app.Close();
}

int main(int argc, char* argv[])
{
	atcp::Application app;
	if (app.Init(argc, argv) != 0)
		EXIT_FAILURE;

	// TODO load a scene that has a single renderable object of primitive triangle 

	std::thread closer(closeAfterDelay, 5, std::ref(app));

	app.Run();

	closer.join();

	return EXIT_SUCCESS;
}