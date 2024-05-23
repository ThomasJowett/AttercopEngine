#include "Application.hpp"
#include "Logger.hpp"
#include "WGPUNameHelpers.hpp"

#define SDL_MAIN_HANDLED
#include <sdl2webgpu.h>
#include <SDL2/SDL.h>

#include <vector>

namespace atcp {

Application::Application()
{
}

Application::~Application()
{
	SDL_Quit();
}

int Application::Init(int, char**)
{
	WGPUInstanceDescriptor desc = {};
	desc.nextInChain = nullptr;

	WGPUInstance instance = wgpuCreateInstance(&desc);

	if (!instance) {
		LOG_CRITICAL("Could not initialize WebGPU!");
		return 1;
	}

	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		LOG_ERROR("Could not initialize SDL! Error: {0}", SDL_GetError());
		return 1;
	}

	LOG_INFO("WGPU instance: {}", fmt::ptr(instance));

	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

	int windowFlags = 0;
	SDL_Window* window = SDL_CreateWindow("Atterop", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, windowFlags);

	WGPUSurface surface = SDL_GetWGPUSurface(instance, window);

	LOG_TRACE("Requesting adapter...");
	WGPURequestAdapterOptions adapterOpts = {};
	adapterOpts.nextInChain = nullptr;
	adapterOpts.compatibleSurface = surface;

	WGPUAdapter adapter = RequestAdapter(instance, &adapterOpts);

	std::vector<WGPUFeatureName> features;

	size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);

	features.resize(featureCount);

	wgpuAdapterEnumerateFeatures(adapter, features.data());

	LOG_DEBUG("Adapter features: ");
	for (auto f : features) {
		LOG_DEBUG(" - {0}", WGPUFeatureNamesToStr(f));
	}

	wgpuSurfaceRelease(surface);
	wgpuAdapterRelease(adapter);
	return 0;
}

void Application::Run()
{
	if (m_Running) {
		LOG_ERROR("Application is already running");
	}

	m_Running = true;

	SDL_Event event;

	while (m_Running)
	{
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
			{
				m_Running = false;
			}
			//else if(event.type == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID())
		}
	}
}
WGPUAdapter Application::RequestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const* options)
{
	struct UserData {
		WGPUAdapter adapter = nullptr;
		bool requestEnded = false;
	};
	UserData userData;

	auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* pUserData) {
		UserData& userData = *reinterpret_cast<UserData*>(pUserData);
		if (status == WGPURequestAdapterStatus_Success) {
			userData.adapter = adapter;
		}
		else {
			LOG_ERROR("Could not get WebGPU adapter: {0}", message);
		}

		userData.requestEnded = true;
	};

	wgpuInstanceRequestAdapter(
		instance,
		options,
		onAdapterRequestEnded,
		(void*)&userData
	);

	ASSERT(userData.requestEnded, "");
	return userData.adapter;
}
}