#pragma once
#include <string>

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
};