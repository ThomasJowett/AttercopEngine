#pragma once

#include "cereal/cereal.hpp"
#include "Scripting/Lua/LuaBindings.h"
#include "Reflection/ReflectionFwd.h"

struct HiddenComponent
{
	HiddenComponent() = default;
	HiddenComponent(const HiddenComponent&) = default;

	bool hidden = true;

	static void Describe(Reflection::Builder<HiddenComponent>& c);

	REFLECT_LUA_BEGIN(HiddenComponent)
		REFLECT_LUA_PROPERTY(hidden, "Whether this entity is hidden from rendering - a hidden parent also hides its children, but doesn't change their own Hidden value")
	REFLECT_LUA_END()

private:
	friend cereal::access;
	template<typename Archive>
	void serialize(Archive& archive)
	{
		archive(hidden);
	}
};

// Set by SceneGraph::Traverse on entities hidden by themselves or an ancestor; never saved
struct HiddenInHierarchyMarker {};
