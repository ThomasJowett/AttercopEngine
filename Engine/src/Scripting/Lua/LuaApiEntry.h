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

	// "(Entity, Vec3)", or "(...)" when the arguments aren't known
	std::string FormatParameters() const
	{
		if (!hasSignature)
			return "(...)";
		std::string result = "(";
		for (size_t i = 0; i < parameters.size(); ++i)
			result += (i > 0 ? ", " : "") + parameters[i];
		return result + ")";
	}
};

// Same as LuaManager::AddApiEntry
void RegisterLuaApiEntry(LuaApiEntry entry);
