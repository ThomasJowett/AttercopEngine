#include "Application.hpp"
#include "Logger.hpp"
#include "SimpleMeshParser.hpp"
#include "Uniforms.hpp"

#define SDL_MAIN_HANDLED
#include <sdl2webgpu.h>
#include <SDL2/SDL.h>

#include <vector>

namespace atcp {

void wgpuPollEvents([[maybe_unused]] wgpu::Device device, [[maybe_unused]] bool yieldToWebBrowser) {
#if defined(WEBGPU_BACKEND_DAWN)
	device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
	device.poll(false);
#elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
	if (yieldToWebBrowser)
	{
		emscripten_sleep(100);
	}
#endif
}

/**
 * Round 'value' up to the next multiplier of 'step'.
 */
uint32_t ceilToNextMultiple(uint32_t value, uint32_t step) {
	uint32_t divide_and_ceil = value / step + (value % step == 0 ? 0 : 1);
	return step * divide_and_ceil;
}

Application::Application()
{
	atcp::Logger::Init("App");
}

Application::~Application()
{
	m_Pipeline.release();
	m_Adapter.release();
	m_Surface.release();
	m_Device.release();
	m_Queue.release();
	m_Instance.release();
	m_VertexBuffer.release();
	SDL_Quit();
}

int Application::Init(int, char* argv[])
{
	m_WorkingDirectory = std::filesystem::weakly_canonical(std::filesystem::path(argv[0])).parent_path();
	std::filesystem::current_path(m_WorkingDirectory);
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

	wgpu::AdapterProperties properties = {};
	m_Adapter.getProperties(&properties);

	LOG_DEBUG("Using GPU: {0}", properties.name);

	LOG_TRACE("Requesting device...");
	wgpu::DeviceDescriptor deviceDesc = {};
	deviceDesc.label = "Main Device";
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "The default queue";
	deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message, void* /*pUserData*/)
		{
			LOG_ERROR("Device lost: Reason: {0} Message: {1}", (int)reason, message);
		};

	wgpu::RequiredLimits requiredLimits = GetRequiredLimits(m_Adapter);
	deviceDesc.requiredLimits = &requiredLimits;

	m_Device = m_Adapter.requestDevice(deviceDesc);

	LOG_DEBUG("Got device");

	auto onDeviceError = [](wgpu::ErrorType type, char const* message)
		{
			LOG_ERROR("Uncaptured device error: type {0}", (int)type);
			if (message)
				LOG_ERROR(message);
		};

	m_ErrorCallbackHandle = m_Device.setUncapturedErrorCallback(std::move(onDeviceError));
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

	wgpu::ShaderModuleDescriptor shaderDesc;

#ifdef WEBGPU_BACKEND_WGPU
	shaderDesc.hintCount = 0;
	shaderDesc.hints = nullptr;
#endif

	wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc;
	shaderCodeDesc.chain.next = nullptr;
	shaderCodeDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
	shaderDesc.nextInChain = &shaderCodeDesc.chain;

	wgpu::ShaderModule shaderModule = LoadShaderModule(m_WorkingDirectory / "resources" / "shader.wgsl");

	wgpu::RenderPipelineDescriptor pipelineDesc;

	wgpu::VertexBufferLayout vertexBufferLayout;
	std::vector<wgpu::VertexAttribute> vertexAttribs(2);
	wgpu::VertexAttribute& positionAttrib = vertexAttribs[0];
	positionAttrib.shaderLocation = 0;
	positionAttrib.format = wgpu::VertexFormat::Float32x2;
	positionAttrib.offset = 0;

	wgpu::VertexAttribute& colorAttrib = vertexAttribs[1];
	colorAttrib.shaderLocation = 1;
	colorAttrib.format = wgpu::VertexFormat::Float32x3;
	colorAttrib.offset = 2 * sizeof(float);

	vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttribs.size());
	vertexBufferLayout.attributes = vertexAttribs.data();
	vertexBufferLayout.arrayStride = 5 * sizeof(float);
	vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;

	pipelineDesc.vertex.bufferCount = 1;
	pipelineDesc.vertex.buffers = &vertexBufferLayout;
	pipelineDesc.vertex.module = shaderModule;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;

	pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
	pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
	pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
	pipelineDesc.primitive.cullMode = wgpu::CullMode::None;

	wgpu::FragmentState fragmentState;
	fragmentState.module = shaderModule;
	fragmentState.entryPoint = "fs_main";
	fragmentState.constantCount = 0;
	fragmentState.constants = nullptr;

	pipelineDesc.fragment = &fragmentState;

	pipelineDesc.depthStencil = nullptr;

	wgpu::BlendState blendState;
	blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
	blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
	blendState.color.operation = wgpu::BlendOperation::Add;

	wgpu::ColorTargetState colorTarget;
	colorTarget.format = surfaceFormat;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = wgpu::ColorWriteMask::All;

	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;

	pipelineDesc.multisample.count = 1;
	pipelineDesc.multisample.mask = ~0u;
	pipelineDesc.multisample.alphaToCoverageEnabled = false;

	wgpu::BindGroupLayoutEntry bindingLayout = wgpu::Default;
	bindingLayout.binding = 0;
	bindingLayout.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
	bindingLayout.buffer.type = wgpu::BufferBindingType::Uniform;
	bindingLayout.buffer.minBindingSize = sizeof(MyUniform);
	bindingLayout.buffer.hasDynamicOffset = true;

	wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc{};
	bindGroupLayoutDesc.entryCount = 1;
	bindGroupLayoutDesc.entries = &bindingLayout;
	wgpu::BindGroupLayout bindGroupLayout = m_Device.createBindGroupLayout(bindGroupLayoutDesc);

	wgpu::PipelineLayoutDescriptor layoutDesc{};
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
	wgpu::PipelineLayout layout = m_Device.createPipelineLayout(layoutDesc);

	pipelineDesc.layout = layout;

	m_Pipeline = m_Device.createRenderPipeline(pipelineDesc);

	uint32_t uniformStride = ceilToNextMultiple(
		(uint32_t)sizeof(MyUniform),
		(uint32_t)m_DeviceLimits.minUniformBufferOffsetAlignment
	);

	wgpu::BufferDescriptor bufferDesc;
	bufferDesc.label = "Uniform Buffer";
	bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
	bufferDesc.size = uniformStride + sizeof(MyUniform);
	bufferDesc.mappedAtCreation = false;
	m_UniformBuffer = m_Device.createBuffer(bufferDesc);

	MyUniform uniforms;
	uniforms.time = 1.0f;
	uniforms.colour = { 0.4f, 0.0f, 1.0f, 1.0f };
	m_Queue.writeBuffer(m_UniformBuffer, 0, &uniforms, sizeof(MyUniform));

	uniforms.time = -1.0f;
	uniforms.colour = { 0.0f, 1.0f, 0.4f, 1.0f };
	m_Queue.writeBuffer(m_UniformBuffer, uniformStride, &uniforms, sizeof(MyUniform));


	wgpu::BindGroupEntry binding{};
	binding.binding = 0;
	binding.buffer = m_UniformBuffer;
	binding.offset = 0;
	binding.size = sizeof(MyUniform);

	wgpu::BindGroupDescriptor bindGroupDesc{};
	bindGroupDesc.layout = bindGroupLayout;
	bindGroupDesc.entryCount = bindGroupLayoutDesc.entryCount;
	bindGroupDesc.entries = &binding;
	m_BindGroup = m_Device.createBindGroup(bindGroupDesc);

	wgpu::CommandEncoder encoder = m_Device.createCommandEncoder(wgpu::Default);

	wgpu::CommandBuffer command = encoder.finish(wgpu::Default);
	encoder.release();
	m_Queue.submit(1, &command);
	command.release();

	InitializeBuffers();

	return 0;
}

void Application::Close()
{
	m_Running = false;
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

		uint32_t uniformStride = ceilToNextMultiple(
			(uint32_t)sizeof(MyUniform),
			(uint32_t)m_DeviceLimits.minUniformBufferOffsetAlignment
		);

		MyUniform uniforms;
		uniforms.time = static_cast<float>(GetTime());
		m_Queue.writeBuffer(m_UniformBuffer, offsetof(MyUniform, time), &uniforms.time, sizeof(float));

		uniforms.time = -static_cast<float>(GetTime()) - 1.0f;
		m_Queue.writeBuffer(m_UniformBuffer, uniformStride + offsetof(MyUniform, time), &uniforms.time, sizeof(float));

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
		renderPass.setPipeline(m_Pipeline);
		renderPass.setVertexBuffer(0, m_VertexBuffer, 0, m_VertexBuffer.getSize());
		renderPass.setIndexBuffer(m_IndexBuffer, wgpu::IndexFormat::Uint16, 0, m_IndexCount * sizeof(uint16_t));

		uint32_t dynamicOffset = 0;

		dynamicOffset = 0 * uniformStride;
		renderPass.setBindGroup(0, m_BindGroup, 1, &dynamicOffset);
		renderPass.drawIndexed(m_IndexCount, 1, 0, 0, 0);

		dynamicOffset = 1 * uniformStride;
		renderPass.setBindGroup(0, m_BindGroup, 1, &dynamicOffset);
		renderPass.drawIndexed(m_IndexCount, 1, 0, 0, 0);

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

wgpu::RequiredLimits Application::GetRequiredLimits(wgpu::Adapter adapter)
{
	wgpu::SupportedLimits supportedLimits;
	adapter.getLimits(&supportedLimits);
	m_DeviceLimits = supportedLimits.limits;

	wgpu::RequiredLimits requiredLimits = wgpu::Default;

	requiredLimits.limits.maxVertexAttributes = 2;
	requiredLimits.limits.maxVertexBuffers = 1;
	requiredLimits.limits.maxBufferSize = 15 * 5 * sizeof(float);
	requiredLimits.limits.maxVertexBufferArrayStride = 5 * sizeof(float);
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.maxTextureDimension2D = supportedLimits.limits.maxTextureDimension2D;
	requiredLimits.limits.maxInterStageShaderComponents = 3;
	requiredLimits.limits.maxBindGroups = 1;
	requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
	requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4;
	requiredLimits.limits.maxDynamicUniformBuffersPerPipelineLayout = 1;

	return requiredLimits;
}
void Application::InitializeBuffers()
{
	std::vector<float> vertexData;

	std::vector<uint16_t> indexData;

	bool success = SimpleMeshParser::LoadGeometry(m_WorkingDirectory / "resources" / "simple_mesh.txt", vertexData, indexData);
	if (!success) {
		LOG_ERROR("Could not load geometry!");
		return;
	}

	m_VertexCount = static_cast<uint32_t>(vertexData.size() / 5);
	m_IndexCount = static_cast<uint32_t>(indexData.size());

	wgpu::BufferDescriptor bufferDesc;
	bufferDesc.size = vertexData.size() * sizeof(float);
	bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
	bufferDesc.mappedAtCreation = false;
	m_VertexBuffer = m_Device.createBuffer(bufferDesc);

	m_Queue.writeBuffer(m_VertexBuffer, 0, vertexData.data(), bufferDesc.size);

	bufferDesc.size = indexData.size() * sizeof(uint16_t);
	bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;
	m_IndexBuffer = m_Device.createBuffer(bufferDesc);

	m_Queue.writeBuffer(m_IndexBuffer, 0, indexData.data(), bufferDesc.size);
}
wgpu::ShaderModule Application::LoadShaderModule(const std::filesystem::path& path)
{
	std::ifstream file(path);
	if (!file.is_open()) {
		LOG_CRITICAL("Could not load shader from {0}", path.string());
		return nullptr;
	}
	file.seekg(0, std::ios::end);
	size_t size = file.tellg();
	std::string shaderSource(size, ' ');
	file.seekg(0);
	file.read(shaderSource.data(), size);

	wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc{};
	shaderCodeDesc.chain.next = nullptr;
	shaderCodeDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
	shaderCodeDesc.code = shaderSource.c_str();
	wgpu::ShaderModuleDescriptor shaderDesc{};
	shaderDesc.hintCount = 0;
	shaderDesc.hints = nullptr;
	shaderDesc.nextInChain = &shaderCodeDesc.chain;
	return m_Device.createShaderModule(shaderDesc);
}
double Application::GetTime()
{
	static Uint64 startCounter = SDL_GetPerformanceCounter();
	Uint64 currentCounter = SDL_GetPerformanceCounter();
	return (currentCounter - startCounter) / (double)SDL_GetPerformanceFrequency();
}
}