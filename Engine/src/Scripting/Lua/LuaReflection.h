#pragma once

#include "sol/sol.hpp"
#include "EnTT/entt.hpp"

#include <string>

class Scene;

namespace Reflection
{
	struct ComponentType;
}

namespace Lua
{
	// What Lua holds for a component: its entity and description, re-resolved through the registry on every access
	struct ComponentRef
	{
		const Reflection::ComponentType* type = nullptr;
		entt::entity entity = entt::null;
		Scene* scene = nullptr;

		// nullptr once the entity is destroyed or the component removed
		void* TryGet() const;
		// Raises a Lua error once the entity is destroyed or the component removed
		void* Get() const;

		template<typename T>
		T& As() const { return *static_cast<T*>(Get()); }

		bool operator==(const ComponentRef& other) const
		{
			return type == other.type && entity == other.entity && scene == other.scene;
		}
	};

	// Given to a component's LuaFunctions: the functions and hand-written properties of its Lua type
	class ComponentFunctions
	{
	public:
		ComponentFunctions(sol::table functions, sol::table properties)
			:m_Functions(functions), m_Properties(properties) {}

		template<typename F>
		void set_function(const std::string& name, F&& function)
		{
			m_Functions.set_function(name, std::forward<F>(function));
		}

		// A property that isn't a described field; getter and setter take const ComponentRef& first
		template<typename Getter, typename Setter>
		void SetProperty(const std::string& name, Getter&& getter, Setter&& setter)
		{
			sol::table property = m_Properties.create(0, 2);
			property.set_function("get", std::forward<Getter>(getter));
			property.set_function("set", std::forward<Setter>(setter));
			m_Properties[name] = property;
		}

	private:
		sol::table m_Functions;
		sol::table m_Properties;
	};

	// The Component usertype, each registered component's accessors on Entity, its properties from its description,
	// its LuaFunctions, and a table for every registered enum
	void BindComponents(sol::state& state);
}
