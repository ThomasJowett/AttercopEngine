#include "Application.h"

#include "Logging/Instrumentor.h"
#include "Settings.h"
#include "InputParser.h"
#include "Version.h"

#include "Renderer/Renderer.h"
#include "Renderer/RenderCommand.h"
#include "Asset/Font.h"

#include "Events/JoystickEvent.h"
#include "Events/SceneEvent.h"

#include "Scene/SceneManager.h"
#include "Scene/AssetManager.h"

#include "Logging/Logger.h"
#include "Input/Input.h"
#include "Input/InputActionSystem.h"

#include "Utilities/StringUtils.h"
#include "Scripting/Lua/LuaManager.h"
#ifdef __EMSCRIPTEN__
#include "Platform/Web/WebGamepads.h"
#endif

#include <chrono>
#include <cstdlib>

#ifndef DEBUG
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif // !DEBUG

#if defined(_WIN32)
#include <crtdbg.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

Application* Application::s_Instance = nullptr;

Application::EventCallbackFn Application::s_EventCallback;

/* ------------------------------------------------------------------------------------------------------------------ */

// The per-user folder for an application's settings and log, or empty if it can't be found
static std::filesystem::path UserDataDirectory(const std::string& name)
{
#if defined(_WIN32)
	wchar_t* appData = nullptr;
	size_t length = 0;
	std::filesystem::path directory;
	if (_wdupenv_s(&appData, &length, L"APPDATA") == 0 && appData && *appData)
		directory = std::filesystem::path(appData) / name;
	free(appData);
	return directory;
#elif defined(__APPLE__)
	const char* home = std::getenv("HOME");
	if (home && *home)
		return std::filesystem::path(home) / "Library" / "Application Support" / name;
#elif !defined(__EMSCRIPTEN__)
	const char* config = std::getenv("XDG_CONFIG_HOME");
	if (config && *config)
		return std::filesystem::path(config) / name;
	const char* home = std::getenv("HOME");
	if (home && *home)
		return std::filesystem::path(home) / ".config" / name;
#endif
	return {};
}

/* ------------------------------------------------------------------------------------------------------------------ */

Application::Application()
{
#if defined(_WIN32)
	// Enable memory-leak reports
	_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG));
#endif

	PROFILE_FUNCTION();
	CORE_ASSERT(!s_Instance, "Application already exists! Cannot create multiple applications");
	s_Instance = this;

	s_EventCallback = BIND_EVENT_FN(Application::OnEvent);
}

/* ------------------------------------------------------------------------------------------------------------------ */

Application::~Application()
{
	PROFILE_FUNCTION();
	m_LayerStack.PushPop();
	// Detach while members and subsystems are still alive; layers save state that uses them
	m_LayerStack.DetachAll();
	SceneManager::Shutdown();
	if (!m_Headless)
		Settings::SaveSettings();
	if (m_Window) {
		if (m_ImGuiManager) m_ImGuiManager->Shutdown();
		Renderer::Shutdown();
		Font::Shutdown();
	}
	AssetManager::Shutdown();
	LuaManager::Shutdown();
}

int Application::Init(int argc, char* argv[], const std::string& userFolderName)
{
	InputParser input(argc, argv);
	return Init(input, userFolderName);
}

int Application::Init(const InputParser& inputParser, const std::string& userFolderName)
{
	const InputParser& input = inputParser;

	// Resolved before the working directory changes below
	std::filesystem::path logPath;
	if (input.CmdOptionExists("--log"))
	{
		const std::string& value = input.GetCmdOption("--log");
		if (!value.empty())
			logPath = std::filesystem::absolute(value);
	}

	m_ExecutablePath = std::filesystem::weakly_canonical(std::filesystem::path(input.RetrieveWorkingDirectory()));
	m_WorkingDirectory = m_ExecutablePath.parent_path();
#ifdef __APPLE__
	if (m_WorkingDirectory.filename() == "MacOS")
	{
		m_WorkingDirectory = m_WorkingDirectory.parent_path() / "Resources";
	}
#endif
	std::filesystem::current_path(m_WorkingDirectory);

	// Scripted/headless testing flags - see the matching getters in Application.h.
	m_Headless = input.CmdOptionExists("--headless");

	m_SettingsDirectory = m_WorkingDirectory;
#ifdef __EMSCRIPTEN__
	// The web Editor's page mounts IndexedDB storage here, so settings survive a reload
	if (std::filesystem::is_directory("/persistent"))
		m_SettingsDirectory = "/persistent";
#else
	// Named after the .app on macOS and the executable elsewhere, so each exported game gets its own folder
	std::string folderName = userFolderName;
	if (folderName.empty())
		folderName = (m_ExecutablePath.parent_path().filename() == "MacOS"
			? m_ExecutablePath.parent_path().parent_path().parent_path() : m_ExecutablePath).stem().string();

	// Headless runs don't use settings, so they don't create a folder
	std::filesystem::path userDirectory = m_Headless ? std::filesystem::path() : UserDataDirectory(folderName);
	std::error_code error;
	if (!userDirectory.empty() && (std::filesystem::create_directories(userDirectory, error) || std::filesystem::is_directory(userDirectory, error)))
		m_SettingsDirectory = userDirectory;

	if (logPath.empty())
		logPath = m_SettingsDirectory / "Log.txt";
#endif
	Logger::Init(logPath);

	if (input.CmdOptionExists("-h") || input.CmdOptionExists("--help"))
	{
		std::cout << "usage:"
			<< " [--help] "
			<< " [--version] "
			<< " [--profile] "
			<< " [--auto-play] "
			<< " [--exit-after <seconds>] "
			<< " [--scene <path>] "
			<< " [--headless] "
			<< " [--fixed-step] "
			<< " [--log <path>] "
			<< std::endl;
		return EXIT_SUCCESS;
	}

	if (input.CmdOptionExists("-v") || input.CmdOptionExists("--version"))
	{
		std::cout << VERSION_MAJOR << '.' << VERSION_MINOR << '.' << VERSION_PATCH << std::endl;
		return EXIT_SUCCESS;
	}

	if (input.CmdOptionExists("-p") || input.CmdOptionExists("--profile"))
	{
	}

	m_FixedStep = input.CmdOptionExists("--fixed-step");

	if (input.CmdOptionExists("--exit-after"))
	{
		const std::string& value = input.GetCmdOption("--exit-after");
		if (!value.empty())
			m_ExitAfterSeconds = std::stod(value);
	}

	if (input.CmdOptionExists("--scene"))
	{
		const std::string& value = input.GetCmdOption("--scene");
		if (!value.empty())
			m_SceneOverride = value;
	}

	// Headless runs keep settings in memory so they never read or write the user's file
	if (!m_Headless)
		Settings::Init();
	SetDefaultSettings();

	std::string file;
	bool hasFile = input.File(file);
	if (hasFile)
		SetOpenDocument(file);

	if (!input.HasFoundAllArguments() && !hasFile)
	{
		std::cerr << "Not a valid input parameter" << std::endl;
		return EXIT_FAILURE;
	}

	Random::Init();
	LuaManager::Init();

	if (m_Headless)
		Input::Init(nullptr);

	if (RenderCommand::CreateRendererAPI() != 0)
		return EXIT_FAILURE;

	ENGINE_INFO("Engine Version: {0}.{1}.{2}", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

	ENGINE_INFO("Engine Initialised");
	return -1;
}

/* ------------------------------------------------------------------------------------------------------------------ */

Window* Application::CreateDesktopWindowImpl(const WindowProps& props)
{
	if (m_Headless)
	{
		ENGINE_ERROR("Cannot create a window in headless mode");
		return nullptr;
	}

	const char* windowStr = props.title.c_str();
	Settings::SetDefaultInt(windowStr, "Window_Width", props.width);
	Settings::SetDefaultInt(windowStr, "Window_Height", props.height);
	Settings::SetDefaultInt(windowStr, "Window_Position_X", props.posX);
	Settings::SetDefaultInt(windowStr, "Window_Position_Y", props.posY);
	Settings::SetDefaultInt(windowStr, "Window_Mode", (int)props.windowMode);
	Settings::SetDefaultBool(windowStr, "Window_Maximized", props.maximized);

	m_Window = CreateScope<Window>(props);
	if (m_Window && m_Window->GetNativeWindow()) {
		m_Window->SetEventCallback(BIND_EVENT_FN(Application::OnEvent));

		if (Settings::GetBool(windowStr, "Window_Maximized"))
		{
			m_Window->MaximizeWindow();
			Renderer::OnWindowResize(m_Window->GetWidth(), m_Window->GetHeight());
		}

		Renderer::Init();
		Font::Init();
		Input::Init(m_Window->GetNativeWindow());

		if (!m_ImGuiManager) {
			m_ImGuiManager = CreateScope<ImGuiManager>();
			m_ImGuiManager->Init();
		}
		return m_Window.get();
	}
	else
		ENGINE_ERROR("Could not create window `{0}`", props.title);
	return nullptr;
}

void Application::Tick() {
	PROFILE_FUNCTION();
	PROFILE_FRAME();

	AssetManager::ProcessPendingFileEvents();

	// --fixed-step: exactly one fixed update per frame, independent of how long the frame took
	double frameTime = m_FixedStep ? (double)m_FixedUpdateInterval : GetTime() - m_CurrentTime;
	m_CurrentTime += frameTime;

	// --exit-after: let this frame finish normally, just don't schedule another one.
	if (m_ExitDeadline >= 0.0 && m_CurrentTime >= m_ExitDeadline)
		Close();

	m_Accumulator += frameTime;

	m_DeltaTime = (float)frameTime;

#ifdef __EMSCRIPTEN__
	WebGamepads::Poll();
#endif
	InputActionSystem::Update();

	// On Fixed update
	while (m_Accumulator >= m_FixedUpdateInterval)
	{
		PROFILE_SCOPE("Layer Stack Fixed Update");

		if (!m_Minimized)
		{
			for (Ref<Layer> layer : m_LayerStack)
			{
				layer->OnFixedUpdate();
			}

			SceneManager::FixedUpdate();
		}

		m_Accumulator -= m_FixedUpdateInterval;
	}

	if (m_Window)
	{
		m_Window->GetContext()->MakeCurrent();
		m_Window->OnUpdate();

		// The first frame is presented by this OnUpdate
		if (m_FrameCount == 1)
		{
			m_Window->InitJoysticks();
			if (m_ImGuiManager)
				m_ImGuiManager->EnableGamepadNavigation();
		}
	}

	// On Update 
	{
		PROFILE_SCOPE("Layer Stack Update");

		// The scene updates first, so the layers render this frame's state
		SceneManager::Update((float)frameTime);

		if (!m_Minimized)
		{
			for (Ref<Layer> layer : m_LayerStack)
			{
				layer->OnUpdate((float)frameTime);
			}
		}
	}

	// Render the imgui of each of the layers
	if (m_ImGuiManager && m_ImGuiManager->IsUsing())
	{
		m_ImGuiManager->Begin();
		{
			PROFILE_SCOPE("Layer Stack ImGui Render");
			for (Ref<Layer> layer : m_LayerStack)
			{
				layer->OnImGuiRender();
			}
		}
		m_ImGuiManager->End();
	}

	m_LayerStack.PushPop();

	// App.Quit from a script: the editor stops Play mode, otherwise the application closes
	if (m_QuitRequested)
	{
		m_QuitRequested = false;
		AppQuitRequestedEvent event;
		CallEvent(event);
		if (!event.Handled)
			Close();
	}

	Input::ClearInputData();
	++m_FrameCount;
}

void Application::Run()
{
	PROFILE_FUNCTION();

	if (IsRunning()) {
		ENGINE_ERROR("Application is already running");
	}

	m_CurrentTime = m_FixedStep ? 0.0 : GetTime();
	m_Accumulator = 0.0;
	m_FrameCount = 0;

	if (m_ExitAfterSeconds >= 0.0)
		m_ExitDeadline = m_CurrentTime + m_ExitAfterSeconds;

	m_Running = true;
#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg([](void* arg) { static_cast<Application*>(arg)->Tick(); }, this, 0, 1);
#else
	while (m_Running)
	{
		Tick();
	}
#endif
}

/* ------------------------------------------------------------------------------------------------------------------ */

void Application::OnEvent(Event& e)
{
	PROFILE_FUNCTION();

	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(Application::OnWindowClose));
	if (!m_Running)
		return;
	dispatcher.Dispatch<WindowMaximizedEvent>(BIND_EVENT_FN(Application::OnMaximize));
	dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Application::OnWindowResize));
	dispatcher.Dispatch<WindowMoveEvent>(BIND_EVENT_FN(Application::OnWindowMove));

	dispatcher.Dispatch<JoystickConnected>([](JoystickConnected& event) {
		ENGINE_INFO(event.to_string());
		return false;
		});
	dispatcher.Dispatch<JoystickDisconnected>([](JoystickDisconnected& event) {
		ENGINE_INFO(event.to_string());
		return false;
		});
	dispatcher.Dispatch<SceneChangedEvent>([](SceneChangedEvent& event) {
		ENGINE_INFO(event.to_string());
		return false;
		});

	if (m_ImGuiManager)
		m_ImGuiManager->OnEvent(e);

	for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
	{
		if (e.Handled)
			break;
		(*it)->OnEvent(e);
	}
}

/* ------------------------------------------------------------------------------------------------------------------ */

bool Application::OnWindowClose(WindowCloseEvent& e)
{
	Close();
	return true;
}

/* ------------------------------------------------------------------------------------------------------------------ */

bool Application::OnWindowResize(WindowResizeEvent& e)
{
	PROFILE_FUNCTION();

	if (e.GetWidth() == 0 || e.GetHeight() == 0)
	{
		m_Minimized = true;
		return false;
	}
	m_Minimized = false;

	if (!Settings::GetBool(m_Window->GetTitle(), "Window_Maximized"))
	{
		Settings::SetInt(m_Window->GetTitle(), "Window_Width", e.GetWidth());
		Settings::SetInt(m_Window->GetTitle(), "Window_Height", e.GetHeight());
	}

	Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());
	return false;
}

/* ------------------------------------------------------------------------------------------------------------------ */

bool Application::OnWindowMove(WindowMoveEvent& e)
{
	if (!Settings::GetBool(m_Window->GetTitle(), "Window_Maximized"))
	{
		Settings::SetInt(m_Window->GetTitle(), "Window_Position_X", e.GetTopLeftX());
		Settings::SetInt(m_Window->GetTitle(), "Window_Position_Y", e.GetTopLeftY());
	}
	return false;
}

/* ------------------------------------------------------------------------------------------------------------------ */

bool Application::OnMaximize(WindowMaximizedEvent& e)
{
	Settings::SetBool(m_Window->GetTitle(), "Window_Maximized", e.IsMaximized());
	return false;
}

/* ------------------------------------------------------------------------------------------------------------------ */

bool Application::SetOpenDocumentImpl(const std::filesystem::path& filepath)
{
	SceneManager::ChangeSceneState(SceneState::Edit);

	if (std::filesystem::exists(filepath))
	{
		m_OpenDocument = filepath;
		m_OpenDocumentDirectory = filepath.parent_path();

		if (filepath.extension() != ".proj")
			return true;

#ifndef __EMSCRIPTEN__
		// The web Editor keeps its own recent projects, since in-memory paths don't survive a reload
		std::string recentFiles = Settings::GetValue("Files", "Recent_Files");

		std::vector<std::string> recentFilesList = SplitString(recentFiles, ',');

		// Canonicalize before comparing/storing - otherwise the same project opened via
		// different routes (e.g. the "Open Recent" list vs. a file dialog) can end up with
		// slightly different path spellings (separators, case, relative vs absolute) and be
		// treated as distinct entries, duplicating the Recent Projects menu.
		std::string fileStr = std::filesystem::weakly_canonical(filepath).string();

		bool containsFile = false;

		for (std::string& file : recentFilesList)
		{
			if (file == fileStr)
			{
				containsFile = true;
				break;
			}
		}

		if (!containsFile)
		{
			recentFiles.append(fileStr + ',');
			Settings::SetValue("Files", "Recent_Files", recentFiles.c_str());
		}
#endif

		AssetManager::CleanUp();
		AssetManager::Init(m_OpenDocumentDirectory);

		if (s_Instance)
		{
			AppOpenDocumentChangedEvent event;
			s_EventCallback(event);
		}

		return true;
	}
	return false;
}

/* ------------------------------------------------------------------------------------------------------------------ */

void Application::SetDefaultSettings()
{
	const char* display = "Display";
	const char* audio = "Audio";

	Settings::SetDefaultBool(display, "V-Sync", true);

	Settings::SetDefaultDouble(audio, "Master", 100.0);

	Settings::SetDefaultValue("Files", "Recent_Files", "");
}

/* ------------------------------------------------------------------------------------------------------------------ */

double Application::GetTime() const
{
	static const auto s_StartTime = std::chrono::steady_clock::now();
	const auto now = std::chrono::steady_clock::now();
	return std::chrono::duration<double>(now - s_StartTime).count();
}

/* ------------------------------------------------------------------------------------------------------------------ */

uint32_t Application::GetGameViewportWidth()
{
	Application& app = Get();
	if (app.m_HasGameViewportOverride)
		return app.m_GameViewportWidth;
	return app.m_Window ? app.m_Window->GetWidth() : HeadlessViewportWidth;
}

uint32_t Application::GetGameViewportHeight()
{
	Application& app = Get();
	if (app.m_HasGameViewportOverride)
		return app.m_GameViewportHeight;
	return app.m_Window ? app.m_Window->GetHeight() : HeadlessViewportHeight;
}

/* ------------------------------------------------------------------------------------------------------------------ */

bool Application::SetOpenDocument(const std::filesystem::path& filepath)
{
	return s_Instance->SetOpenDocumentImpl(filepath);
}

/* ------------------------------------------------------------------------------------------------------------------ */

const std::filesystem::path& Application::GetOpenDocument()
{
	return s_Instance->m_OpenDocument;
}

/* ------------------------------------------------------------------------------------------------------------------ */

const std::filesystem::path& Application::GetOpenDocumentDirectory()
{
	return s_Instance->m_OpenDocumentDirectory;
}

const std::filesystem::path& Application::GetWorkingDirectory()
{
	return s_Instance->m_WorkingDirectory;
}

/* ------------------------------------------------------------------------------------------------------------------ */

const std::filesystem::path& Application::GetSettingsDirectory()
{
	return s_Instance->m_SettingsDirectory;
}

/* ------------------------------------------------------------------------------------------------------------------ */

const std::filesystem::path& Application::GetExecutablePath()
{
	return s_Instance->m_ExecutablePath;
}
