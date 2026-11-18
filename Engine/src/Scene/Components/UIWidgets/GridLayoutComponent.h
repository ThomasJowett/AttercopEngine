#pragma once

#include "math/Vector2f.h"
#include "Reflection/ReflectionFwd.h"

struct GridLayoutComponent
{
	GridLayoutComponent() = default;
	GridLayoutComponent(const GridLayoutComponent&) = default;

	int columns = 2;
	Vector2f cellSpacing = Vector2f(0.0f, 0.0f);

	float paddingLeft = 0.0f;
	float paddingTop = 0.0f;
	float paddingRight = 0.0f;
	float paddingBottom = 0.0f;

	bool uniformCellSize = true;
	float fixedRowHeight = 0.0f;

	static void Describe(Reflection::Builder<GridLayoutComponent>& c);
};
