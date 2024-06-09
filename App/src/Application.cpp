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
	m_SwapChain.release();
	m_Adapter.release();
	m_Surface.release();
	m_Device.release();
	m_Queue.release();
	m_Instance.release();
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

	m_Adapter = m_Instance.requestAdapter(adapterOpts);

	size_t featureCount = m_Adapter.enumerateFeatures(nullptr);

	std::vector<WGPUFeatureName> features;
	features.resize(featureCount);

	wgpuAdapterEnumerateFeatures(m_Adapter, features.data());

	LOG_DEBUG("Adapter features: ");
	for (auto f : features) {
		LOG_DEBUG(" - {0}", WGPUFeatureNamesToStr(f));
	}

	LOG_TRACE("Requesting device...");
	wgpu::DeviceDescriptor deviceDesc = {};
	deviceDesc.label = "Main Device";
	deviceDesc.requiredFeaturesCount = 0;
	deviceDesc.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "The default queue";
	deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message, void* /*pUserData*/) {
		LOG_ERROR("Device lost: Reason: {0} Message: {1}", (int)reason, message);
	};
	m_Device = m_Adapter.requestDevice(deviceDesc);

	LOG_DEBUG("Got device");

	auto onDeviceError = [](wgpu::ErrorType type, char const* message)
	{
		LOG_ERROR("Uncaptured device error: type {0}", (int)type);
		if (message)
			LOG_ERROR(message);
	};

	m_Device.setUncapturedErrorCallback(onDeviceError);

	wgpu::SwapChainDescriptor swapChainDesc = {};
	swapChainDesc.width = windowWidth;
	swapChainDesc.height = windowHeight;

	wgpu::TextureFormat swapChainFormat = m_Surface.getPreferredFormat(m_Adapter);
	swapChainDesc.format = swapChainFormat;
	swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
	swapChainDesc.presentMode = wgpu::PresentMode::Fifo;

	LOG_TRACE("Creating swap chain...");
	m_SwapChain = m_Device.createSwapChain(m_Surface, swapChainDesc);
	m_Queue = m_Device.getQueue();

	auto onQueueWorkDone = [](wgpu::QueueWorkDoneStatus status)
	{
		LOG_INFO("Queued work finished with status: {0}", WGPUQueueWorkDoneStatusToStr(status));
	};
	m_Queue.onSubmittedWorkDone(onQueueWorkDone);

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

		wgpu::TextureView nextTexture = m_SwapChain.getCurrentTextureView();
		if (!nextTexture)
		{
			LOG_ERROR("Cannot acquire next swap chain texture!");
			continue;
		}

		wgpu::CommandEncoderDescriptor encoderDesc = {};
		encoderDesc.label = "Command encoder";
		wgpu::CommandEncoder encoder = m_Device.createCommandEncoder(encoderDesc);

		wgpu::RenderPassDescriptor renderPassDesc = {};

		wgpu::RenderPassColorAttachment renderPassColorAttachment = {};
		renderPassColorAttachment.view = nextTexture;
		renderPassColorAttachment.resolveTarget = nullptr;
		renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
		renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
		renderPassColorAttachment.clearValue = wgpu::Color{ 0.1, 0.4, 0.1, 1.0 };

		renderPassDesc.colorAttachmentCount = 1;
		renderPassDesc.colorAttachments = &renderPassColorAttachment;
		renderPassDesc.depthStencilAttachment = nullptr;
		renderPassDesc.timestampWriteCount = 0;
		renderPassDesc.timestampWrites = nullptr;
		renderPassDesc.nextInChain = nullptr;

		wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
		renderPass.end();
		renderPass.release();

		// Encode commands into a command buffer
		wgpu::CommandBufferDescriptor cmdBufferDescriptor = {};
		cmdBufferDescriptor.label = "Command buffer";
		wgpu::CommandBuffer command = encoder.finish(cmdBufferDescriptor);
		encoder.release();

		m_Queue.submit(command);
		command.release();
		nextTexture.release();
		m_SwapChain.present();
	}
}
}