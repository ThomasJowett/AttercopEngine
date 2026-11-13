#include "SceneTestLayer.h"

#include "Core/Application.h"
#include "Scene/SceneManager.h"
#include "Scripting/Lua/LuaManager.h"
#include "Scripting/Lua/LuaBindings.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Components/LuaScriptComponent.h"

SceneTestLayer::SceneTestLayer(const SceneTestOptions& options)
	:Layer("SceneTest"), m_Options(options)
{
}

void SceneTestLayer::OnAttach()
{
	if (m_Options.IsRenderMode())
	{
		FrameBufferSpecification spec = { m_Options.width, m_Options.height };
		spec.attachments = { FrameBufferTextureFormat::RGBA8, FrameBufferTextureFormat::RED_INTEGER, FrameBufferTextureFormat::Depth };
		m_RenderTarget = FrameBuffer::Create(spec);
		CLIENT_INFO("SceneTest: rendering {0}x{1} on {2}, goldens in {3}, output in {4}", m_Options.width, m_Options.height,
			m_Options.api, m_Options.goldenDirectory.string(), m_Options.outputDirectory.string());
	}

	// Loaded here rather than before Run(), so errors raised while the scripts load are counted
	SceneManager::ChangeSceneState(SceneState::Play);
	m_SceneLoaded = SceneManager::ChangeScene(m_Options.scene);
	if (!m_SceneLoaded)
	{
		CLIENT_ERROR("RESULT: FAIL could not load scene {0}", m_Options.scene.string());
		Application::RequestQuit();
	}
}

void SceneTestLayer::OnUpdate(float deltaTime)
{
	if (++m_Frames == m_Options.maxFrames)
	{
		CLIENT_INFO("SceneTest: reached the frame limit of {0}", m_Options.maxFrames);
		Application::RequestQuit();
	}

	Scene* scene = SceneManager::CurrentScene();
	if (!scene)
		return;

	if (m_RenderTarget)
	{
		Renderer2D::ResetStats();
		auto [view, projection] = scene->GetPrimaryCameraViewProjection();
		m_RenderTarget->Bind();
		RenderCommand::Clear();
		m_RenderTarget->ClearAttachment(1, -1);
		Renderer::RenderScene(scene, view, projection, m_RenderTarget);
		m_RenderTarget->UnBind();

		// OnDebugRender draws over the scene, as in the editor's Play mode
		m_RenderTarget->Bind();
		RenderCommand::StartRenderPass();
		Renderer::BeginScene(view, projection);
		scene->GetRegistry().view<LuaScriptComponent>().each([](auto entity, LuaScriptComponent& script) { script.OnDebugRender(); });
		Renderer::EndScene();
		RenderCommand::EndRenderPass();
		m_RenderTarget->UnBind();

		CaptureScreenshots();
	}

	if (m_Options.display)
	{
		Renderer2D::ResetStats();
		Renderer::RenderScene(scene);
	}
}

void SceneTestLayer::Screenshot(const std::string& name, const ImageDiffLimits& limits)
{
	if (!m_Options.IsRenderMode())
	{
		CLIENT_WARN("SceneTest: Test.Screenshot(\"{0}\") ignored, render tests need --api", name);
		return;
	}
	m_PendingScreenshots.push_back({ name, limits });
}

void SceneTestLayer::CaptureScreenshots()
{
	if (m_PendingScreenshots.empty())
		return;

	Image actual;
	actual.width = m_Options.width;
	actual.height = m_Options.height;
	bool read = m_RenderTarget->ReadPixels(0, actual.rgba);

	for (const PendingScreenshot& screenshot : m_PendingScreenshots)
	{
		const std::string& name = screenshot.name;
		if (!read)
		{
			++m_ImageMismatches;
			CLIENT_ERROR("RESULT: FAIL screenshot {0} could not be read back", name);
			continue;
		}

		std::filesystem::path golden = m_Options.goldenDirectory / (name + ".png");
		std::filesystem::path actualPath = m_Options.outputDirectory / (name + ".actual.png");
		if (!m_Options.outputDirectory.empty() && !actual.Save(actualPath))
			CLIENT_WARN("SceneTest: could not write {0}", actualPath.string());

		if (m_Options.updateGoldens)
		{
			if (actual.Save(golden))
				CLIENT_INFO("RESULT: OK screenshot {0} wrote golden {1}", name, golden.string());
			else
			{
				++m_ImageMismatches;
				CLIENT_ERROR("RESULT: FAIL screenshot {0} could not write golden {1}", name, golden.string());
			}
			continue;
		}

		Image expected;
		if (!expected.Load(golden))
		{
			++m_ImageMismatches;
			CLIENT_ERROR("RESULT: FAIL screenshot {0} has no golden image {1}", name, golden.string());
			continue;
		}

		ImageDiffResult result = CompareImages(actual, expected, screenshot.limits);
		if (result.passed)
		{
			CLIENT_INFO("RESULT: OK screenshot {0} matches its golden: {1}", name, result.ToString());
			continue;
		}

		if (m_Options.strict)
		{
			++m_ImageMismatches;
			CLIENT_ERROR("RESULT: FAIL screenshot {0} differs from its golden: {1}", name, result.ToString());
		}
		else
			CLIENT_WARN("RESULT: WARN screenshot {0} differs from its golden: {1} (fails with --strict)", name, result.ToString());
		if (!m_Options.outputDirectory.empty())
		{
			expected.Save(m_Options.outputDirectory / (name + ".expected.png"));
			MakeDiffImage(actual, expected, screenshot.limits.pixelThreshold).Save(m_Options.outputDirectory / (name + ".diff.png"));
		}
	}
	m_PendingScreenshots.clear();
}

void SceneTestLayer::OnEvent(Event& event)
{
	EventDispatcher dispatcher(event);
	dispatcher.Dispatch<LuaErrorEvent>(BIND_EVENT_FN(SceneTestLayer::OnLuaError));
	dispatcher.Dispatch<AppQuitRequestedEvent>(BIND_EVENT_FN(SceneTestLayer::OnQuitRequested));
}

bool SceneTestLayer::OnLuaError(LuaErrorEvent& event)
{
	++m_LuaErrors;
	CLIENT_WARN("SceneTest: Lua error {0}: {1}", m_LuaErrors, event.to_string());
	return false;
}

bool SceneTestLayer::OnQuitRequested(AppQuitRequestedEvent& event)
{
	// Stop the scene while the application is still running, so errors in OnDestroy are counted
	SceneManager::Shutdown();
	Application::Get().Close();
	return true;
}

void SceneTestLayer::Expect(bool condition, const std::string& message)
{
	if (condition)
		CLIENT_INFO("RESULT: OK {0}", message);
	else
	{
		++m_FailedExpectations;
		CLIENT_ERROR("RESULT: FAIL {0}", message);
	}
}

void SceneTestLayer::Pass(const std::string& message)
{
	CLIENT_INFO("RESULT: OK {0}", message);
	m_Passed = true;
	Application::RequestQuit();
}

void SceneTestLayer::Fail(const std::string& message)
{
	CLIENT_ERROR("RESULT: FAIL {0}", message);
	m_Failed = true;
	Application::RequestQuit();
}

SceneTestResult SceneTestLayer::GetResult() const
{
	CLIENT_INFO("SceneTest: {0} frames, {1} failed expectations, {2} of {3} expected Lua errors",
		m_Frames, m_FailedExpectations, m_LuaErrors, m_Options.expectedErrors);

	if (!m_SceneLoaded || m_Failed || m_FailedExpectations > 0)
		return SceneTestResult::Failed;
	if (m_ImageMismatches > 0 || !m_PendingScreenshots.empty())
	{
		CLIENT_ERROR("SceneTest: {0} screenshots did not match, {1} were never taken", m_ImageMismatches, m_PendingScreenshots.size());
		return SceneTestResult::ImageMismatch;
	}
	if (m_LuaErrors != m_Options.expectedErrors)
	{
		CLIENT_ERROR("SceneTest: expected {0} Lua errors but {1} were raised", m_Options.expectedErrors, m_LuaErrors);
		return SceneTestResult::ErrorCountMismatch;
	}
	if (m_Options.requireVerdict && !m_Passed)
	{
		CLIENT_ERROR("SceneTest: the scene ended without calling Test.Pass or Test.Fail");
		return SceneTestResult::NoVerdict;
	}
	return SceneTestResult::Passed;
}

static std::string ToString(const sol::object& value)
{
	return LuaManager::GetState()["tostring"](value);
}

void SceneTestLayer::BindTestTable(SceneTestLayer* layer)
{
	sol::state& state = LuaManager::GetState();
	sol::table test = state.create_named_table("Test");
	LuaManager::AddIdentifier("Test", "Assertions for automated scene tests, only available under SceneTest");

	Lua::SetFunction(test, "Test", "Expect", "Record a failure if the condition is false, and keep running",
		[layer](sol::object condition, sol::optional<std::string> message)
		{
			// Lua truthiness: only nil and false fail
			bool truthy = condition.get_type() != sol::type::lua_nil
				&& !(condition.get_type() == sol::type::boolean && !condition.as<bool>());
			layer->Expect(truthy, message.value_or("Test.Expect"));
		});
	Lua::SetFunction(test, "Test", "Equal", "Record a failure if the two values are not equal, and keep running",
		[layer](sol::this_state s, sol::object actual, sol::object expected, sol::optional<std::string> message)
		{
			lua_State* L = s;
			actual.push(L);
			expected.push(L);
			bool equal = lua_compare(L, -2, -1, LUA_OPEQ) != 0;
			lua_pop(L, 2);

			std::string text = message.value_or("Test.Equal");
			if (!equal)
				text += " (expected " + ToString(expected) + ", got " + ToString(actual) + ")";
			layer->Expect(equal, text);
		});
	Lua::SetFunction(test, "Test", "Pass", "End the run as passed, unless an expectation has failed",
		[layer](sol::optional<std::string> message) { layer->Pass(message.value_or("Test.Pass")); });
	Lua::SetFunction(test, "Test", "Fail", "End the run as failed",
		[layer](sol::optional<std::string> message) { layer->Fail(message.value_or("Test.Fail")); });
	Lua::SetFunction(test, "Test", "Screenshot", "Compare this frame's image with the golden image of the same name; limits are optional",
		[layer](const std::string& name, sol::optional<sol::table> limits)
		{
			ImageDiffLimits diffLimits;
			if (limits)
			{
				diffLimits.pixelThreshold = limits->get_or("PixelThreshold", diffLimits.pixelThreshold);
				diffLimits.maxDifferentFraction = limits->get_or("MaxDifferentFraction", diffLimits.maxDifferentFraction);
				diffLimits.maxChannelDifference = limits->get_or("MaxChannelDifference", diffLimits.maxChannelDifference);
			}
			layer->Screenshot(name, diffLimits);
		});
	Lua::SetFunction(test, "Test", "ExpectErrors", "Declare how many Lua errors this scene raises on purpose",
		[layer](int count) { layer->m_Options.expectedErrors = count; });
}
