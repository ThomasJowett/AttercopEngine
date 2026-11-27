#pragma once

#include "Reflection/ComponentType.h"
#include "Reflection/Value.h"
#include "Core/UUID.h"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class Entity;

namespace Reflection
{
	// "TilingFactor" to "Tiling Factor", for inspector labels
	std::string SplitWords(const std::string& name);

	// Every registered component, in registration order: the order of a scene file and the inspector
	const std::vector<const ComponentType*>& Components();
	// By XML element name, "Sprite"; nullptr if none
	const ComponentType* FindComponent(std::string_view name);
	// A component on the entity that the type can't be added beside; nullptr if none
	const ComponentType* ExcludedBy(entt::registry& registry, entt::entity entity, const ComponentType& type);

	const std::vector<const EnumType*>& Enums();
	// By name, "BodyType"; nullptr if none
	const EnumType* FindEnum(std::string_view name);

	namespace Detail
	{
		ComponentType& AddComponent();
		EnumType& AddEnum(std::string name, std::string description, std::vector<EnumType::Entry> entries);

		template<typename T>
		inline const ComponentType* s_ComponentType = nullptr;

		template<typename E>
		inline const EnumType* s_EnumType = nullptr;
	}

	// The description of a registered component
	template<typename T>
	const ComponentType& TypeOf()
	{
		CORE_ASSERT(Detail::s_ComponentType<T>, "Component isn't registered with Reflection");
		return *Detail::s_ComponentType<T>;
	}

	template<typename T>
	bool IsRegistered() { return Detail::s_ComponentType<T> != nullptr; }

	// The names of a registered enum
	template<typename E>
	const EnumType& EnumOf()
	{
		CORE_ASSERT(Detail::s_EnumType<E>, "Enum isn't registered with Reflection");
		return *Detail::s_EnumType<E>;
	}

	// Registers an enum's names, once; later calls return the first registration
	template<typename E>
	const EnumType& Enum(const char* name, const char* description, std::initializer_list<std::pair<const char*, E>> entries)
	{
		static_assert(std::is_enum_v<E>, "Reflection::Enum needs an enum type");
		if (!Detail::s_EnumType<E>)
		{
			std::vector<EnumType::Entry> list;
			for (const std::pair<const char*, E>& entry : entries)
				list.push_back({ entry.first, (int32_t)entry.second });
			Detail::s_EnumType<E> = &Detail::AddEnum(name, description, std::move(list));
		}
		return *Detail::s_EnumType<E>;
	}

	// A property as indices into the descriptions: Transform.Position, Transform.Position.X, PolygonCollider2D.Vertices[2].Y
	struct PropertyPath
	{
		const ComponentType* component = nullptr;
		int field = -1;
		// List item, or -1
		int index = -1;
		// Part of a vector or colour, or -1
		int part = -1;

		// nullopt, with a warning, if a name isn't known or the text isn't a path
		static std::optional<PropertyPath> Parse(std::string_view text);
		std::string ToString() const;

		const FieldInfo& Field() const { return component->fields[field]; }
		// The type of the value the path names
		ValueType Type() const;

		bool operator==(const PropertyPath& other) const
		{
			return component == other.component && field == other.field && index == other.index && part == other.part;
		}
		bool operator!=(const PropertyPath& other) const { return !(*this == other); }
	};

	enum class ChangeSource { Editor, Undo, Lua, Sequence, Network, Load };

	// Sent by the scene for each property Set or Restore changes
	struct PropertyChanged
	{
		Uuid entity;
		PropertyPath path;
		Value oldValue;
		Value newValue;
		ChangeSource source;
		// The same for every change of one inspector edit, such as a drag; 0 for changes not made in the inspector
		uint32_t interaction = 0;
	};

	// nullopt if the entity doesn't have the component or the list item doesn't exist
	std::optional<Value> Get(Entity entity, const PropertyPath& path);

	// The one way generic code writes a property: checks the value fits, does nothing if it's equal, runs the
	// field's then the component's OnChanged, marks the scene changed when it isn't running and sends PropertyChanged
	bool Set(Entity entity, const PropertyPath& path, const Value& value, ChangeSource source, uint32_t interaction = 0);

	struct ComponentValues
	{
		const ComponentType* type = nullptr;
		// One per saved field that isn't custom, in field order
		std::vector<Value> values;
		// Custom fields, as XML text
		std::string custom;
	};

	ComponentValues Capture(Entity entity, const ComponentType& type);
	// Adds the component if it's missing, sets every field that isn't read-only, then runs each OnChanged hook once
	void Restore(Entity entity, const ComponentValues& values, ChangeSource source, uint32_t interaction = 0);
	// From a default-constructed component
	ComponentValues Defaults(const ComponentType& type);

	// Whether the target has one of the components an entity field requires; any entity does if it lists none
	bool MeetsRequirements(const FieldInfo& field, Entity target);
	// Points the entity fields of the entity's components, lists included, at the new ID of any target in ids,
	// writing them directly as loading does
	void RemapEntities(Entity entity, const std::unordered_map<Uuid, Uuid>& ids);
}
