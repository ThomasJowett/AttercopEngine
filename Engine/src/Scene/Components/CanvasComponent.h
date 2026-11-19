#pragma once

#include "Reflection/ReflectionFwd.h"

struct CanvasComponent
{
	CanvasComponent() = default;
	CanvasComponent(const CanvasComponent&) = default;

	float pixelPerUnit = 1.0f;

	static void Describe(Reflection::Builder<CanvasComponent>& c);
};
