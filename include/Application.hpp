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

private:
	bool m_Running = false;
	float m_FixedUpdateInterval = 0.01f;

	static Application* s_Instance;
	friend int ::main(int argc, char* argv[]);
};
}

#endif // APPLICATION_HPP
