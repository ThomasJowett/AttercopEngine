#pragma once

#include "Core/Colour.h"
#include "Reflection/ReflectionFwd.h"

struct CircleRendererComponent
{
	Colour colour{ 1.0f, 1.0f, 1.0f, 1.0f };
	float radius = 0.5f;
	float thickness = 1.0f;
	float fade = 0.005f;

	CircleRendererComponent() = default;
	CircleRendererComponent(const CircleRendererComponent&) = default;
	static void Describe(Reflection::Builder<CircleRendererComponent>& c);
};