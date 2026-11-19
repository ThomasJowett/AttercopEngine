#pragma once

#include "Reflection/ReflectionFwd.h"

struct HiddenComponent
{
	HiddenComponent() = default;
	HiddenComponent(const HiddenComponent&) = default;

	bool hidden = true;

	static void Describe(Reflection::Builder<HiddenComponent>& c);
};

// Set by SceneGraph::Traverse on entities hidden by themselves or an ancestor; never saved
struct HiddenInHierarchyMarker {};
