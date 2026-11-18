#pragma once

#include "Reflection/ReflectionFwd.h"

struct StackLayoutComponent
{
	StackLayoutComponent() = default;
	StackLayoutComponent(const StackLayoutComponent&) = default;

	bool horizontal = false;

	float spacing = 0.0f;
	bool stretchCrossAxis = true;

	float paddingLeft = 0.0f;
	float paddingTop = 0.0f;
	float paddingRight = 0.0f;
	float paddingBottom = 0.0f;

	static void Describe(Reflection::Builder<StackLayoutComponent>& c);
};
