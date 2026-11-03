#pragma once

#include "Renderer/RendererAPI.h"

// Renderer API for headless runs, every command does nothing
class NullRendererAPI : public RendererAPI
{
public:
	virtual bool Init() override { return true; }
	virtual void SetClearColour(const Colour& colour) override {}
	virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override {}
	virtual Vector2f GetViewportSize() override { return Vector2f(1.0f, 1.0f); }
	virtual void Clear() override {}
	virtual void ClearColour() override {}
	virtual void ClearDepth() override {}
	virtual void StartRenderPass() override {}
	virtual void EndRenderPass() override {}
	virtual void DrawIndexed(uint32_t indexCount = 0, uint32_t startIndex = 0, uint32_t vertexOffset = 0) override {}
	virtual void DrawLines(uint32_t vertexCount = 0) override {}
};
