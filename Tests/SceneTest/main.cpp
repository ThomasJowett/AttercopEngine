#include <algorithm>
#include <chrono>
#include <cstdio>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "Core/Application.h"
#include "Logging/Logger.h"
#include "Core/InputParser.h"
#include "Input/InputActionSystem.h"
#include "Input/InputMappings.h"
#include "Physics/CollisionLayers.h"
#include "Scene/SceneManager.h"
#include "Renderer/RendererAPI.h"
#include "Core/Settings.h"

#include "SceneTestLayer.h"
#include "ImageDiff.h"

// Compares two PNGs, or every *.actual.png in one folder with the same file in another; 0 if all match
static int CompareImageFiles(const std::filesystem::path& a, const std::filesystem::path& b, const std::filesystem::path& output)
{
	std::vector<std::pair<std::filesystem::path, std::filesystem::path>> pairs;
	if (std::filesystem::is_directory(a))
	{
		for (const auto& entry : std::filesystem::directory_iterator(a))
		{
			std::string file = entry.path().filename().string();
			if (file.size() > 11 && file.compare(file.size() - 11, 11, ".actual.png") == 0)
				pairs.emplace_back(entry.path(), b / file);
		}
		std::sort(pairs.begin(), pairs.end());
	}
	else
		pairs.emplace_back(a, b);

	if (pairs.empty())
	{
		std::cerr << "SceneTest: no images to compare in " << a.string() << std::endl;
		return EXIT_FAILURE;
	}

	int mismatches = 0;
	for (const auto& [first, second] : pairs)
	{
		Image imageA, imageB;
		if (!imageA.Load(first) || !imageB.Load(second))
		{
			std::cout << "FAIL " << first.filename().string() << ": could not load both images" << std::endl;
			++mismatches;
			continue;
		}
		ImageDiffResult result = CompareImages(imageA, imageB, ImageDiffLimits());
		std::cout << (result.passed ? "OK   " : "FAIL ") << first.filename().string() << ": " << result.ToString() << std::endl;
		if (!result.passed)
		{
			++mismatches;
			if (!output.empty() && result.sizeMatches)
			{
				std::string stem = first.filename().string();
				stem = stem.substr(0, stem.find('.'));
				MakeDiffImage(imageA, imageB, 0).Save(output / (stem + ".diff.png"));
			}
		}
	}
	return mismatches == 0 ? (int)SceneTestResult::Passed : (int)SceneTestResult::ImageMismatch;
}

// Runs one scene headless with a fixed time step and reports the result as the exit code, see SceneTestResult
int main(int argc, char* argv[])
{
	InputParser input(argc, argv);

	if (input.CmdOptionExists("-h") || input.CmdOptionExists("--help"))
	{
		std::cout << "usage: SceneTest --project <file.proj> --scene <file.scene> [--frames N] [--timeout S]"
			" [--log <path>] [--expect-errors N] [--require-verdict] [--display]\n"
			"       render tests: [--api <opengl|webgpu>] [--size 320x180] [--golden <dir>] [--output <dir>] [--update-goldens] [--strict]\n"
			"       SceneTest --compare <a.png|dir> --against <b.png|dir> [--output <dir>]" << std::endl;
		return EXIT_SUCCESS;
	}

	auto option = [&input](const std::string& name)
		{
			return input.CmdOptionExists(name) ? input.GetCmdOption(name) : std::string();
		};

	if (input.CmdOptionExists("--compare"))
		return CompareImageFiles(option("--compare"), option("--against"), option("--output"));

	std::filesystem::path project = option("--project");
	SceneTestOptions options;
	options.scene = option("--scene");
	std::string log = option("--log");
	std::string frames = option("--frames");
	std::string timeout = option("--timeout");
	std::string expectErrors = option("--expect-errors");
	options.requireVerdict = input.CmdOptionExists("--require-verdict");
	options.display = input.CmdOptionExists("--display");
	options.updateGoldens = input.CmdOptionExists("--update-goldens");
	options.strict = input.CmdOptionExists("--strict");
	std::string api = option("--api");
	std::string size = option("--size");
	// Read each option once, InputParser counts every lookup
	std::string golden = option("--golden");
	std::string output = option("--output");
	if (!golden.empty())
		options.goldenDirectory = std::filesystem::absolute(golden);
	if (!output.empty())
		options.outputDirectory = std::filesystem::absolute(output);

	if (api == "opengl")
		options.api = "OpenGL";
	else if (api == "webgpu")
		options.api = "WebGPU";
	else if (!api.empty())
	{
		std::cerr << "SceneTest: --api must be opengl or webgpu" << std::endl;
		return EXIT_FAILURE;
	}
	if (!size.empty() && std::sscanf(size.c_str(), "%ux%u", &options.width, &options.height) != 2)
	{
		std::cerr << "SceneTest: --size must look like 320x180" << std::endl;
		return EXIT_FAILURE;
	}

	if (project.empty() || options.scene.empty() || !input.HasFoundAllArguments())
	{
		std::cerr << "SceneTest: --project and --scene are required, see --help" << std::endl;
		return EXIT_FAILURE;
	}

	// Resolved before Application::Init changes the working directory
	project = std::filesystem::absolute(project);
	if (std::filesystem::exists(options.scene))
		options.scene = std::filesystem::absolute(options.scene);
	if (!frames.empty())
		options.maxFrames = std::stoull(frames);
	if (!expectErrors.empty())
		options.expectedErrors = std::stoi(expectErrors);
	double timeoutSeconds = timeout.empty() ? 60.0 : std::stod(timeout);

	std::vector<std::string> appArgs = { argv[0] };
	if (options.IsRenderMode())
	{
		// The settings file is never read
		appArgs.insert(appArgs.end(), { "--no-settings", "--fixed-step", "--api", options.api });
	}
	else if (!options.display)
	{
		appArgs.push_back("--headless");
		appArgs.push_back("--fixed-step");
	}
	if (!log.empty())
	{
		appArgs.push_back("--log");
		appArgs.push_back(log);
	}
	std::vector<char*> appArgv;
	for (std::string& arg : appArgs)
		appArgv.push_back(arg.data());

	Ref<Application> app = CreateRef<Application>();
	int rCode = app->Init((int)appArgv.size(), appArgv.data());
	if (rCode != -1)
		return rCode;

	if (options.IsRenderMode())
	{
		// Hidden unless --display; the window's size is the scene's viewport size
		Settings::SetBool("Display", "V-Sync", false);
		WindowProps props(options.scene.filename().string() + " - " + RendererAPI::GetAPIName(), options.width, options.height, 100, 100);
		props.visible = options.display;
		if (!app->CreateDesktopWindow(props))
		{
			CLIENT_ERROR("SceneTest: could not create a window for {0}", options.api);
			return EXIT_FAILURE;
		}
	}
	else if (options.display)
	{
		std::string windowTitle = options.scene.filename().string() + std::string(" - ") + RendererAPI::GetAPIName();
		app->CreateDesktopWindow(WindowProps(windowTitle, 1920, 1080, 100, 100));
	}

	if (!Application::SetOpenDocument(project))
	{
		CLIENT_ERROR("SceneTest: could not open project {0}", project.string());
		return EXIT_FAILURE;
	}

	std::filesystem::path projectDirectory = Application::GetOpenDocumentDirectory();
	InputActionSystem::LoadMappings(projectDirectory / InputMappings::FilePath);
	CollisionLayers::LoadNames(projectDirectory / CollisionLayers::FilePath);

	Ref<SceneTestLayer> layer = CreateRef<SceneTestLayer>(options);
	SceneTestLayer::BindTestTable(layer.get());
	app->GetLayerStack().AddLayer(layer);

	// A safety net for hangs outside Lua, which the script watchdog can't stop
	std::mutex mutex;
	std::condition_variable finished;
	bool done = false;
	std::thread timeoutThread([&]()
		{
			std::unique_lock<std::mutex> lock(mutex);
			if (!finished.wait_for(lock, std::chrono::duration<double>(timeoutSeconds), [&]() { return done; }))
			{
				CLIENT_ERROR("RESULT: FAIL timed out after {0} seconds", timeoutSeconds);
				Logger::GetClientLogger()->flush();
				std::_Exit((int)SceneTestResult::TimedOut);
			}
		});

	app->Run();

	{
		std::lock_guard<std::mutex> lock(mutex);
		done = true;
	}
	finished.notify_one();
	timeoutThread.join();

	int result = (int)layer->GetResult();
	Logger::GetClientLogger()->flush();
	return result;
}
