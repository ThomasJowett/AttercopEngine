#pragma once

#include "Reflection/Value.h"

#include "EnTT/entt.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class Entity;

namespace tinyxml2
{
	class XMLElement;
}

namespace Reflection
{
	enum class FieldFlags : uint16_t
	{
		None = 0,
		ReadOnly = 1 << 0,
		NotSaved = 1 << 1,
		NoLua = 1 << 2,
		HideInInspector = 1 << 3,
		MainAsset = 1 << 4,
		AsText = 1 << 5,
		Custom = 1 << 6,
		// Inspector hints
		// Radians shown in degrees; the description leaves the unit out, as each side adds its own
		Degrees = 1 << 7,
		LockParts = 1 << 8,
		EditFromStart = 1 << 9,
	};

	inline FieldFlags operator|(FieldFlags a, FieldFlags b) { return (FieldFlags)((uint16_t)a | (uint16_t)b); }
	inline FieldFlags& operator|=(FieldFlags& a, FieldFlags b) { return a = a | b; }
	inline bool HasFlag(FieldFlags flags, FieldFlags flag) { return ((uint16_t)flags & (uint16_t)flag) != 0; }

	enum class ComponentFlags : uint8_t
	{
		None = 0,
		Required = 1 << 0,
		Internal = 1 << 1,
	};

	inline ComponentFlags operator|(ComponentFlags a, ComponentFlags b) { return (ComponentFlags)((uint8_t)a | (uint8_t)b); }
	inline ComponentFlags& operator|=(ComponentFlags& a, ComponentFlags b) { return a = a | b; }
	inline bool HasFlag(ComponentFlags flags, ComponentFlags flag) { return ((uint8_t)flags & (uint8_t)flag) != 0; }

	// A typed function pointer from a description, stored untyped and called through a thunk that casts it back
	using ErasedFunction = void(*)();

	struct Predicate
	{
		ErasedFunction function = nullptr;
		bool (*call)(ErasedFunction function, const void* component) = nullptr;

		explicit operator bool() const { return function != nullptr; }
		bool operator()(const void* component) const { return !function || call(function, component); }
	};

	struct Choices
	{
		ErasedFunction function = nullptr;
		std::vector<std::string> (*call)(ErasedFunction function, const void* component) = nullptr;

		explicit operator bool() const { return function != nullptr; }
		std::vector<std::string> operator()(const void* component) const { return call(function, component); }
	};

	struct ChangeHook
	{
		ErasedFunction function = nullptr;
		void (*call)(ErasedFunction function, Entity entity, void* component) = nullptr;

		explicit operator bool() const { return function != nullptr; }
		void operator()(Entity entity, void* component) const;
	};

	struct GroupInfo
	{
		std::string name;	// "Torus"
		Predicate when;
	};

	struct FieldInfo
	{
		// The Lua property name and the name in a property path; a grouped field's is its group's name followed by its own
		std::string name;
		// The XML attribute or element name; a grouped field's own name, inside its group's element
		std::string xmlName;
		std::string label;
		std::string description;

		ValueType type = ValueType::Bool;
		// The type of each item of a list
		ValueType itemType = ValueType::Bool;
		FieldFlags flags = FieldFlags::None;

		// For Enum, or a list of enums
		const EnumType* enumType = nullptr;
		// The C++ asset type, e.g. "Texture2D", for Asset, or a list of assets
		std::string assetType;
		// Element name of each item of a list
		std::string itemName;

		bool hasRange = false;
		float min = 0.0f;
		float max = 0.0f;
		float speed = 0.0f;

		// Index into ComponentType::groups, or -1
		int group = -1;

		Value (*get)(const void* component) = nullptr;
		// False, with the component unchanged, if the value doesn't fit
		bool (*set)(void* component, const Value& value) = nullptr;
		// Used by Reflection::Set instead of set when there is one; loading and Restore use set
		bool (*edit)(void* component, const Value& value) = nullptr;

		Predicate showIf;
		// Greyed out in the inspector unless it holds
		Predicate enableIf;
		// The values the inspector offers for a string
		Choices choices;
		// Written to files only when it holds; read whenever it's there
		Predicate saveIf;
		ChangeHook onChanged;

		// For .Custom fields: written into and read from the component's XML element
		void (*save)(const void* component, tinyxml2::XMLElement* element) = nullptr;
		void (*load)(void* component, const tinyxml2::XMLElement* element) = nullptr;

		bool Has(FieldFlags flag) const { return HasFlag(flags, flag); }
		bool IsSaved() const { return !Has(FieldFlags::NotSaved); }
		bool IsCustom() const { return Has(FieldFlags::Custom); }
	};

	struct ComponentType
	{
		std::string name;	// The XML element name, "Sprite"
		std::string luaName;	// The C++ and Lua type name, "SpriteComponent"
		std::string category;
		std::string description;
		ComponentFlags flags = ComponentFlags::None;
		// Names of components that can't be on the same entity
		std::vector<std::string> excludes;

		std::vector<FieldInfo> fields;
		std::vector<GroupInfo> groups;

		ChangeHook onChanged;
		// Registers the component's Lua functions; called by the Lua bindings with the component's usertype
		ErasedFunction luaFunctions = nullptr;

		void* (*tryGet)(entt::registry& registry, entt::entity entity) = nullptr;
		void* (*emplace)(entt::registry& registry, entt::entity entity) = nullptr;
		void (*remove)(entt::registry& registry, entt::entity entity) = nullptr;
		// A default-constructed component, for Defaults
		std::shared_ptr<void> (*create)() = nullptr;

		bool Has(ComponentFlags flag) const { return HasFlag(flags, flag); }
		bool IsInternal() const { return Has(ComponentFlags::Internal); }

		// -1 if there's no field with this name
		int FindField(std::string_view fieldName) const;
	};
}
