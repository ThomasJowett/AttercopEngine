#pragma once

#include "RendererAPI.h"

class RenderCommand
{
public:
	// Create a Renderer API object
	static int CreateRendererAPI();

	// Initialise the Renderer
	inline static bool Init() { return s_RendererAPI->Init(); }

	// Sets the clear colour of the screen or currently bound frame buffer
	inline static void SetClearColour(const Colour& colour) { s_RendererAPI->SetClearColour(colour); }

	// Sets the area of the screen that will be rendered to
	inline static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) { return s_RendererAPI->SetViewport(x, y, width, height); }

	// Resets the bound target's colour and depth; call before StartRenderPass
	inline static void Clear() { s_RendererAPI->Clear(); }

	// Resets the bound target's colour to the clear colour; call before StartRenderPass
	inline static void ClearColour() { s_RendererAPI->ClearColour(); }

	// Resets the bound target's depth; call before StartRenderPass
	inline static void ClearDepth() { s_RendererAPI->ClearDepth(); }

	// Starts drawing to the bound target; its contents are kept unless one of the clears above was called
	inline static void StartRenderPass() { s_RendererAPI->StartRenderPass(); }

	inline static void EndRenderPass() { s_RendererAPI->EndRenderPass(); }

	// Draws primitives from the vertex array
	inline static void DrawIndexed(uint32_t indexCount = 0, uint32_t startIndex = 0, uint32_t vertexOffset = 0)
	{
		s_RendererAPI->DrawIndexed(indexCount, startIndex, vertexOffset);
	}

	inline static void DrawLines(uint32_t vertexCount = 0) { s_RendererAPI->DrawLines(vertexCount); }

	inline static RendererAPI& Get() { return *s_RendererAPI; }

private:
	static Scope<RendererAPI> s_RendererAPI;
};