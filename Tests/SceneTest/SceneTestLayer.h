#pragma once

#include <filesystem>
#include <vector>

#include "Core/Layer.h"
#include "Events/ApplicationEvent.h"
#include "Logging/Instrumentor.h"
#include "Scripting/Lua/LuaErrorEvent.h"
#include "Renderer/FrameBuffer.h"

#include "ImageDiff.h"

// Process exit codes reported by SceneTest
enum class SceneTestResult
{
	Passed = 0,
	Failed = 1,
	ErrorCountMismatch = 2,
	NoVerdict = 3,
	TimedOut = 4,
	ImageMismatch = 5
};

struct SceneTestOptions
{
	std::filesystem::path scene;
	uint64_t maxFrames = 600;
	int expectedErrors = 0;
	bool requireVerdict = false;
	bool display = false;

	// Render mode, set by --api: draws each frame into an offscreen target for Test.Screenshot
	std::string api;
	uint32_t width = 320;
	uint32_t height = 180;
	std::filesystem::path goldenDirectory;
	std::filesystem::path outputDirectory;
	bool updateGoldens = false;
	// Without it, a difference from the golden is a warning
	bool strict = false;

	bool IsRenderMode() const { return !api.empty(); }
};

// Loads a scene, plays it, and records Test.* calls and Lua errors until the run ends
class SceneTestLayer : public Layer
{
public:
	explicit SceneTestLayer(const SceneTestOptions& options);

	virtual void OnAttach() override;
	virtual void OnUpdate(float deltaTime) override;
	virtual void OnEvent(Event& event) override;

	// Classifies the run, call once it has ended
	SceneTestResult GetResult() const;

	// Registers the Lua Test table
	static void BindTestTable(SceneTestLayer* layer);

private:
	bool OnLuaError(LuaErrorEvent& event);
	bool OnQuitRequested(AppQuitRequestedEvent& event);

	void Expect(bool condition, const std::string& message);
	void Pass(const std::string& message);
	void Fail(const std::string& message);
	void Screenshot(const std::string& name, const ImageDiffLimits& limits);
	void CaptureScreenshots();

	SceneTestOptions m_Options;
	uint64_t m_Frames = 0;
	int m_LuaErrors = 0;
	int m_FailedExpectations = 0;
	bool m_SceneLoaded = false;
	bool m_Passed = false;
	bool m_Failed = false;

	struct PendingScreenshot
	{
		std::string name;
		ImageDiffLimits limits;
	};
	Ref<FrameBuffer> m_RenderTarget;
	std::vector<PendingScreenshot> m_PendingScreenshots;
	int m_ImageMismatches = 0;
};
