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
	wgpu::TextureView GetNextSurfaceTextureView();

private:
	bool m_Running = false;
	float m_FixedUpdateInterval = 0.01f;

	wgpu::Instance m_Instance = nullptr;
	wgpu::Adapter m_Adapter = nullptr;
	wgpu::Surface m_Surface = nullptr;
	wgpu::Device m_Device = nullptr;
	wgpu::Queue m_Queue = nullptr;
	wgpu::RenderPipeline m_Pipeline = nullptr;

	static Application* s_Instance;
	friend int ::main(int argc, char* argv[]);
};
}

#endif // APPLICATION_HPP
