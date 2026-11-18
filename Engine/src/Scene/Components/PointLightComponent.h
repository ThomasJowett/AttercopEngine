#pragma once

#include "Core/Colour.h"
#include "Reflection/ReflectionFwd.h"

struct PointLightComponent
{
	Colour colour = Colours::WHITE;
	bool castsShadows = true;
	float range = 10.0f;
	float attenuation = 1.0f;
	PointLightComponent() = default;
	PointLightComponent(const PointLightComponent&) = default;

	static void Describe(Reflection::Builder<PointLightComponent>& c);
};
