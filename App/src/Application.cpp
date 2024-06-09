#include "Application.hpp"
#include "Logger.hpp"

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

	LOG_TRACE("Requesting device...");
	wgpu::DeviceDescriptor deviceDesc = {};
	deviceDesc.label = "Main Device";
	deviceDesc.requiredFeatureCount = 0;
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
	m_Queue = m_Device.getQueue();

	wgpu::SurfaceConfiguration surfaceConfig = {};

	surfaceConfig.width = windowWidth;
	surfaceConfig.height = windowHeight;
	surfaceConfig.usage = wgpu::TextureUsage::RenderAttachment;
	wgpu::TextureFormat surfaceFormat = m_Surface.getPreferredFormat(m_Adapter);
	surfaceConfig.format = surfaceFormat;

	surfaceConfig.viewFormatCount = 0;
	surfaceConfig.viewFormats = nullptr;
	surfaceConfig.device = m_Device;
	surfaceConfig.presentMode = wgpu::PresentMode::Fifo;
	surfaceConfig.alphaMode = wgpu::CompositeAlphaMode::Auto;

	m_Surface.configure(surfaceConfig);

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

		wgpu::TextureView targetView = GetNextSurfaceTextureView();
		if (!targetView)
		{
			continue;
		}

		wgpu::CommandEncoderDescriptor encoderDesc = {};
		encoderDesc.label = "Command encoder";
		wgpu::CommandEncoder encoder = m_Device.createCommandEncoder(encoderDesc);

		wgpu::RenderPassDescriptor renderPassDesc = {};

		wgpu::RenderPassColorAttachment renderPassColorAttachment = {};
		renderPassColorAttachment.view = targetView;
		renderPassColorAttachment.resolveTarget = nullptr;
		renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
		renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
		renderPassColorAttachment.clearValue = wgpu::Color{ 0.1, 0.4, 0.1, 1.0 };

		renderPassDesc.colorAttachmentCount = 1;
		renderPassDesc.colorAttachments = &renderPassColorAttachment;
		renderPassDesc.depthStencilAttachment = nullptr;
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
		targetView.release();
		m_Surface.present();

#if defined(WEBGPU_BACKEND_DAWN)
		m_Device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
		m_Device.poll(false);
#endif
	}
}
wgpu::TextureView Application::GetNextSurfaceTextureView()
{
	wgpu::SurfaceTexture surfaceTexture;
	m_Surface.getCurrentTexture(&surfaceTexture);
	if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::Success) {
		LOG_ERROR("Could not get surface texture");
		return nullptr;
	}

	wgpu::Texture texture = surfaceTexture.texture;

	wgpu::TextureViewDescriptor viewDescriptor;
	viewDescriptor.label = "Surface texture view";
	viewDescriptor.format = texture.getFormat();
	viewDescriptor.dimension = wgpu::TextureViewDimension::_2D;
	viewDescriptor.baseMipLevel = 0;
	viewDescriptor.mipLevelCount = 1;
	viewDescriptor.baseArrayLayer = 0;
	viewDescriptor.arrayLayerCount = 1;
	viewDescriptor.aspect = wgpu::TextureAspect::All;
	wgpu::TextureView targetView = texture.createView(viewDescriptor);

	return targetView;
}
}