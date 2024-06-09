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
	wgpuSwapChainRelease(m_SwapChain);
	wgpuAdapterRelease(m_Adapter);
	wgpuSurfaceRelease(m_Surface);
	wgpuDeviceRelease(m_Device);
	wgpuQueueRelease(m_Queue);
	SDL_Quit();
}

int Application::Init(int, char**)
{
	m_Instance = wgpu::createInstance(wgpu::InstanceDescriptor{});

	if (!m_Instance)
	{
		LOG_CRITICAL("Could not initialize WebGPU!");
		return 1;
	}

	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		LOG_ERROR("Could not initialize SDL! Error: {0}", SDL_GetError());
		return 1;
	}

	LOG_TRACE("WGPU instance created");

	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

	int windowFlags = 0;
	int windowWidth = 640;
	int windowHeight = 480;
	SDL_Window* window = SDL_CreateWindow("Atterop", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, windowWidth, windowHeight, windowFlags);

	m_Surface = SDL_GetWGPUSurface(m_Instance, window);

	LOG_TRACE("Requesting adapter...");
	wgpu::RequestAdapterOptions adapterOpts{};
	adapterOpts.compatibleSurface = m_Surface;
	// WGPURequestAdapterOptions adapterOpts = {};
	// adapterOpts.nextInChain = nullptr;
	// adapterOpts.compatibleSurface = m_Surface;

	m_Adapter = RequestAdapter(m_Instance, &adapterOpts);

	std::vector<WGPUFeatureName> features;

	size_t featureCount = wgpuAdapterEnumerateFeatures(m_Adapter, nullptr);

	features.resize(featureCount);

	wgpuAdapterEnumerateFeatures(m_Adapter, features.data());

	LOG_DEBUG("Adapter features: ");
	for (auto f : features) {
		LOG_DEBUG(" - {0}", WGPUFeatureNamesToStr(f));
	}

	LOG_TRACE("Requesting device...");
	WGPUDeviceDescriptor deviceDesc = {};
	deviceDesc.nextInChain = nullptr;
	deviceDesc.label = "Main Device";
	deviceDesc.requiredFeaturesCount = 0;
	deviceDesc.requiredLimits = nullptr;
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "The default queue";
	m_Device = RequestDevice(m_Adapter, &deviceDesc);

	LOG_DEBUG("Got device");

	auto onDeviceError = [](WGPUErrorType type, char const* message, void* /*pUserData*/)
		{
			LOG_ERROR("Uncaptured device error: type {0}", (int)type);
			if (message)
				LOG_ERROR(message);
		};
	wgpuDeviceSetUncapturedErrorCallback(m_Device, onDeviceError, nullptr);

	WGPUSwapChainDescriptor swapChainDesc = {};
	swapChainDesc.nextInChain = nullptr;
	swapChainDesc.width = windowWidth;
	swapChainDesc.height = windowHeight;

	WGPUTextureFormat swapChainFormat = wgpuSurfaceGetPreferredFormat(m_Surface, m_Adapter);
	swapChainDesc.format = swapChainFormat;
	swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
	swapChainDesc.presentMode = WGPUPresentMode_Fifo;

	LOG_TRACE("Creating swap chain...");
	m_SwapChain = wgpuDeviceCreateSwapChain(m_Device, m_Surface, &swapChainDesc);

	m_Queue = wgpuDeviceGetQueue(m_Device);

	auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void* /* pUserData */)
		{
			LOG_INFO("Queued work finished with status: {0}", WGPUQueueWorkDoneStatusToStr(status));
		};
	wgpuQueueOnSubmittedWorkDone(m_Queue, onQueueWorkDone, nullptr /* pUserData */);

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

		WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(m_SwapChain);
		if (!nextTexture)
		{
			LOG_ERROR("Cannot acquire next swap chain texture!");
			continue;
		}

		WGPUCommandEncoderDescriptor encoderDesc = {};
		encoderDesc.nextInChain = nullptr;
		encoderDesc.label = "Command encoder";
		WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_Device, &encoderDesc);

		WGPURenderPassDescriptor renderPassDesc = {};

		WGPURenderPassColorAttachment renderPassColorAttachment = {};
		renderPassColorAttachment.view = nextTexture;
		renderPassColorAttachment.resolveTarget = nullptr;
		renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
		renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
		renderPassColorAttachment.clearValue = WGPUColor{ 0.1, 0.4, 0.1, 1.0 };

		renderPassDesc.colorAttachmentCount = 1;
		renderPassDesc.colorAttachments = &renderPassColorAttachment;
		renderPassDesc.depthStencilAttachment = nullptr;
		renderPassDesc.timestampWriteCount = 0;
		renderPassDesc.timestampWrites = nullptr;
		renderPassDesc.nextInChain = nullptr;

		WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
		wgpuRenderPassEncoderEnd(renderPass);
		wgpuRenderPassEncoderRelease(renderPass);

		// Encode commands into a command buffer
		WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
		cmdBufferDescriptor.nextInChain = nullptr;
		cmdBufferDescriptor.label = "Command buffer";
		WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
		wgpuCommandEncoderRelease(encoder);

		wgpuCommandEncoderRelease(encoder);
		LOG_TRACE("Submitting command...");
		wgpuQueueSubmit(m_Queue, 1, &command);
		wgpuCommandBufferRelease(command);

		wgpuTextureViewRelease(nextTexture);

		wgpuSwapChainPresent(m_SwapChain);
	}
}
wgpu::Adapter Application::RequestAdapter(wgpu::Instance instance, wgpu::RequestAdapterOptions const* options)
{
	struct UserData {
		wgpu::Adapter adapter = nullptr;
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
WGPUDevice Application::RequestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor)
{
	struct UserData
	{
		WGPUDevice device = nullptr;
		bool requestEnded = false;
	};

	UserData userData;

	auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* pUserData)
		{
			UserData& userData = *reinterpret_cast<UserData*>(pUserData);
			if (status == WGPURequestDeviceStatus_Success)
			{
				userData.device = device;
			}
			else
			{
				LOG_ERROR("Could not get WebGPU device: {0}", message);
			}
			userData.requestEnded = true;
		};

	wgpuAdapterRequestDevice(
		adapter,
		descriptor,
		onDeviceRequestEnded,
		(void*)&userData);

	ASSERT(userData.requestEnded, "");
	return userData.device;
}
}