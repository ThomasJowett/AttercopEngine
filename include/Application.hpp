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
	wgpu::Adapter m_Adapter = nullptr;
	wgpu::Surface m_Surface = nullptr;
	wgpu::Device m_Device = nullptr;
	wgpu::SwapChain m_SwapChain = nullptr;
	wgpu::Queue m_Queue = nullptr;

	static Application* s_Instance;
	friend int ::main(int argc, char* argv[]);
};
}

#endif // APPLICATION_HPP
