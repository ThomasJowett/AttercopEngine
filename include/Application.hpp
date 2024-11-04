#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <webgpu/webgpu.hpp>
#include <filesystem>
#include <atomic>

int main(int argc, char* argv[]);

namespace atcp {
class Application {
public:
	Application();
	Application(const Application&) = delete;
	virtual ~Application();

	int Init(int argc, char* argv[]);

	void Close();

private:
	void Run();
	wgpu::TextureView GetNextSurfaceTextureView();
	wgpu::RequiredLimits GetRequiredLimits(wgpu::Adapter adapter);
	void InitializeBuffers();
	wgpu::ShaderModule LoadShaderModule(const std::filesystem::path& path);

	double GetTime();

private:
	std::atomic<bool> m_Running = false;
	float m_FixedUpdateInterval = 0.01f;

	wgpu::Instance m_Instance = nullptr;
	wgpu::Adapter m_Adapter = nullptr;
	wgpu::Surface m_Surface = nullptr;
	wgpu::Device m_Device = nullptr;
	wgpu::Queue m_Queue = nullptr;
	wgpu::RenderPipeline m_Pipeline = nullptr;
	wgpu::Limits m_DeviceLimits;

	static Application* s_Instance;
	friend int ::main(int argc, char* argv[]);

	wgpu::Buffer m_VertexBuffer;
	uint32_t m_VertexCount;
	wgpu::Buffer m_IndexBuffer;
	uint32_t m_IndexCount;

	wgpu::Buffer m_UniformBuffer;
	wgpu::BindGroup m_BindGroup;

	std::filesystem::path m_WorkingDirectory;

	std::unique_ptr<wgpu::ErrorCallback> m_ErrorCallbackHandle;
};
}

#endif // APPLICATION_HPP
