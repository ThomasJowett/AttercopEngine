#pragma once
#include "Scene/Entity.h"
#include "Reflection/ReflectionFwd.h"

struct HierarchyComponent
{
	entt::entity parent = entt::null;
	entt::entity firstChild = entt::null;
	entt::entity previousSibling = entt::null;
	entt::entity nextSibling = entt::null;
	bool isActive = true;

	static void Describe(Reflection::Builder<HierarchyComponent>& c);
};
