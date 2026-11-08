#pragma once
#include <string>
#include <vector>

// Kept free of LuaManager.h (and its Entity/Scene includes) so component headers can use
// the REFLECT_LUA_* macros without an include cycle.
struct LuaApiEntry
{
	enum class Kind { Global, Property, Function, ComponentAccessor, EnumValue };	// EnumValue: description holds the value

	std::string name;
	std::string description;
	Kind kind = Kind::Global;
	std::string component;
	std::string type;	// for functions, what they return
	bool isComponent = false;
	std::vector<std::string> parameters;	// Lua type of each argument, filled in for functions
	bool hasSignature = false;			// false when the arguments can't be worked out, e.g. variadic
	std::vector<std::string> parameterNames;	// optional, one per argument
	std::string documentedOn;			// enums: the page they're listed on, e.g. Input; empty for a page of their own

	// "(prefab: Entity, position: Vec3)", "(Entity, Vec3)" without names, or "(...)" when nothing is known
	std::string FormatParameters() const
	{
		bool named = !parameterNames.empty() && (!hasSignature || parameterNames.size() == parameters.size());
		if (!hasSignature && !named)
			return "(...)";

		size_t count = hasSignature ? parameters.size() : parameterNames.size();
		std::string result = "(";
		for (size_t i = 0; i < count; ++i)
		{
			result += i > 0 ? ", " : "";
			if (named)
				result += parameterNames[i] + (hasSignature ? ": " : "");
			if (hasSignature)
				result += parameters[i];
		}
		return result + ")";
	}
};

// Same as LuaManager::AddApiEntry
void RegisterLuaApiEntry(LuaApiEntry entry);
