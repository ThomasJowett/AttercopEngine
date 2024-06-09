#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <webgpu/webgpu.hpp>

int main(int argc, char* argv[]);

namespace atcp {
class Application {
public:
	Application();
	Application(const Application&) = delete;
	virtual ~Application();

	int Init(int argc, char* argv[]);

private:
	void Run();

	wgpu::Adapter RequestAdapter(wgpu::Instance instance, wgpu::RequestAdapterOptions const* options);
	wgpu::Device RequestDevice(wgpu::Adapter adapter, wgpu::DeviceDescriptor const* descriptor);

private:
	bool m_Running = false;
	float m_FixedUpdateInterval = 0.01f;

	wgpu::Instance m_Instance = nullptr;
	// wgpu::Adapter m_Adapter;
	// wgpu::Surface m_Surface;
	// wgpu::Device m_Device;
	// wgpu::SwapChain m_SwapChain;
	// wgpu::Queue m_Queue;

	// WGPUInstance m_Instance = nullptr;
	WGPUAdapter m_Adapter = nullptr;
	WGPUSurface m_Surface = nullptr;
	WGPUDevice m_Device = nullptr;
	WGPUSwapChain m_SwapChain = nullptr;
	WGPUQueue m_Queue = nullptr;

	static Application* s_Instance;
	friend int ::main(int argc, char* argv[]);
};
}

#endif // APPLICATION_HPP
