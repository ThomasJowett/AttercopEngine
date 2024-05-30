#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <webgpu/webgpu.h>

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

	WGPUAdapter RequestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const* options);
	WGPUDevice RequestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor);

private:
	bool m_Running = false;
	float m_FixedUpdateInterval = 0.01f;

	WGPUInstance m_Instance = nullptr;
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
