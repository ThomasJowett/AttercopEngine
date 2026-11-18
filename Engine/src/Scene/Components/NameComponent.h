#pragma once
#include <string>

#include "Scripting/Lua/LuaBindings.h"
#include "Reflection/ReflectionFwd.h"

struct NameComponent
{
	std::string name;

	NameComponent() = default;
	NameComponent(const NameComponent& name) = default;
	NameComponent(const std::string& name)
		:name(name) {}

	operator std::string& () { return name; }
	operator const std::string& () const { return name; }

	static void Describe(Reflection::Builder<NameComponent>& c);

	REFLECT_LUA_BEGIN(NameComponent)
		REFLECT_LUA_PROPERTY(name, "The name of this entity")
	REFLECT_LUA_END()
};