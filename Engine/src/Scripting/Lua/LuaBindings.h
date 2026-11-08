#pragma once
#include "sol/sol.hpp"
#include "LuaApiEntry.h"
#include "Core/core.h"

#include "Core/Colour.h"
#include "math/Vector2f.h"
#include "math/Vector3f.h"
#include "math/Vector4f.h"
#include "EnTT/entt.hpp"

#include <cctype>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>

class Scene;
class Entity;
class Quaternion;
class Uuid;
class Material;
class PhysicsMaterial;
class SceneCamera;
class Texture;
class Texture2D;
class AudioClip;
class PostProcessEffect;
struct HitResult2D;
namespace BehaviourTree { class BehaviourTree; class Blackboard; }

namespace Lua
{
entt::registry& GetSceneRegistry(Scene* scene);

// Brings the entity's world transform up to date, for bindings that read it straight after a script moves something
void UpdateWorldTransform(Scene* scene, entt::entity entity);

// The calling script's "chunk:line: " prefix, so errors raised from C++ report the Lua line in the error panel
std::string ScriptLocation(lua_State* state);

// The engine's main Lua state, for bindings that can't take sol::this_state (sol counts it as a property argument)
lua_State* MainLuaState();

// Raises a Lua error if the entity was destroyed or never found, naming the function that was called
void CheckEntityValid(const Entity& entity, const char* function, lua_State* state);

// What Lua holds for a component: the entity it belongs to, re-resolved through the registry on every access
template<typename T>
struct ComponentHandle
{
	entt::entity entity = entt::null;
	Scene* scene = nullptr;

	inline static std::string s_LuaName;

	T* TryGet() const
	{
		if (!scene)
			return nullptr;
		entt::registry& registry = GetSceneRegistry(scene);
		return registry.valid(entity) ? registry.try_get<T>(entity) : nullptr;
	}

	T& Get() const
	{
		if (T* component = TryGet())
			return *component;
		throw std::runtime_error(ScriptLocation(MainLuaState()) + s_LuaName + " is no longer valid: its entity was destroyed or the component was removed");
	}
};

namespace Detail
{
template<typename... Args> struct TypeList {};
template<typename T> struct TypeTag { using Type = T; };

template<typename Signature> struct MemberSignature;
template<typename C, typename R, typename... Args>
struct MemberSignature<R(C::*)(Args...)> { using Return = R; using Arguments = TypeList<Args...>; };
template<typename C, typename R, typename... Args>
struct MemberSignature<R(C::*)(Args...) const> { using Return = R; using Arguments = TypeList<Args...>; };
template<typename C, typename R, typename... Args>
struct MemberSignature<R(C::*)(Args...) noexcept> { using Return = R; using Arguments = TypeList<Args...>; };
template<typename C, typename R, typename... Args>
struct MemberSignature<R(C::*)(Args...) const noexcept> { using Return = R; using Arguments = TypeList<Args...>; };

template<typename List> struct DropFirst;
template<typename First, typename... Rest>
struct DropFirst<TypeList<First, Rest...>> { using Type = TypeList<Rest...>; };

template<typename Self, typename F, typename R, typename... Args>
auto MakeResolver(F function, TypeTag<R>, TypeList<Args...>)
{
	return [function](const ComponentHandle<Self>& handle, Args... args) -> std::decay_t<R>
	{
		return std::invoke(function, handle.Get(), std::forward<Args>(args)...);
	};
}
}

// Adapts a binding written against Self& (a member function, or a lambda taking Self& first) to take a
// ComponentHandle<Self>, resolving the live component on each call. Results are returned by value.
template<typename Self, typename F>
auto ResolveOnAccess(F function)
{
	if constexpr (std::is_member_function_pointer_v<F>)
	{
		using Signature = Detail::MemberSignature<F>;
		return Detail::MakeResolver<Self>(function, Detail::TypeTag<typename Signature::Return>{}, typename Signature::Arguments{});
	}
	else
	{
		using Signature = Detail::MemberSignature<decltype(&F::operator())>;
		return Detail::MakeResolver<Self>(function, Detail::TypeTag<typename Signature::Return>{}, typename Detail::DropFirst<typename Signature::Arguments>::Type{});
	}
}
}

// Lua-facing type name for a reflected property, used by ScriptView's autocomplete and
// the Lua API doc generator. Primitive/enum types resolve automatically; anything else
// falls back to LuaTypeNameTrait, which defaults to "unknown" until given a friendlier
// name via LUA_TYPE_NAME (e.g. for asset Ref<T> types or other engine structs).
template<typename T>
struct LuaTypeNameTrait
{
	static std::string Get() { return "unknown"; }
};

#define LUA_TYPE_NAME(CppType, LuaName) \
	template<> struct LuaTypeNameTrait<CppType> { static std::string Get() { return LuaName; } };

template<typename T>
struct LuaTypeNameTrait<Ref<T>>
{
	static std::string Get() { return LuaTypeNameTrait<T>::Get(); }
};

template<typename T>
struct LuaTypeNameTrait<T*>
{
	static std::string Get() { return LuaTypeNameTrait<std::remove_cv_t<T>>::Get(); }
};

template<typename T>
struct LuaTypeNameTrait<sol::optional<T>>
{
	static std::string Get() { return LuaTypeNameTrait<std::decay_t<T>>::Get(); }
};

template<typename T>
struct LuaTypeNameTrait<Lua::ComponentHandle<T>>
{
	static std::string Get() { return Lua::ComponentHandle<T>::s_LuaName; }
};

LUA_TYPE_NAME(std::string, "string")
LUA_TYPE_NAME(std::string_view, "string")
LUA_TYPE_NAME(const char*, "string")
LUA_TYPE_NAME(sol::object, "any")
LUA_TYPE_NAME(sol::table, "table")
LUA_TYPE_NAME(sol::function, "function")
LUA_TYPE_NAME(Colour, "Colour")
LUA_TYPE_NAME(Vector2f, "Vec2")
LUA_TYPE_NAME(Vector3f, "Vec3")
LUA_TYPE_NAME(Vector4f, "Vector4f")
LUA_TYPE_NAME(Quaternion, "Quaternion")
LUA_TYPE_NAME(Uuid, "UUID")
LUA_TYPE_NAME(Entity, "Entity")
LUA_TYPE_NAME(Scene, "Scene")
LUA_TYPE_NAME(Material, "Material")
LUA_TYPE_NAME(PhysicsMaterial, "PhysicsMaterial")
LUA_TYPE_NAME(SceneCamera, "Camera")
LUA_TYPE_NAME(Texture, "Texture")
LUA_TYPE_NAME(Texture2D, "Texture")
LUA_TYPE_NAME(AudioClip, "AudioClip")
LUA_TYPE_NAME(PostProcessEffect, "PostProcessEffect")
LUA_TYPE_NAME(HitResult2D, "HitResult2D")
LUA_TYPE_NAME(BehaviourTree::BehaviourTree, "BehaviourTree")
LUA_TYPE_NAME(BehaviourTree::Blackboard, "Blackboard")

template<typename T>
std::string LuaTypeName()
{
	using Decayed = std::decay_t<T>;
	if constexpr (std::is_same_v<Decayed, bool>)
		return "boolean";
	else if constexpr (std::is_floating_point_v<Decayed>)
		return "number";
	else if constexpr (std::is_enum_v<Decayed>)
		return "integer (enum)";
	else if constexpr (std::is_integral_v<Decayed>)
		return "integer";
	else
		return LuaTypeNameTrait<Decayed>::Get();
}

// Lua property names are PascalCase, like functions, whatever the C++ member is called
inline std::string LuaPropertyName(const char* member)
{
	std::string name = member;
	if (!name.empty())
		name[0] = (char)std::toupper((unsigned char)name[0]);
	return name;
}

// Reflection macros for automatic Lua bindings registration inside components. A
// description is required on every property/function so the same declaration doubles as
// documentation - see LuaApiEntry (LuaApiEntry.h) for what this feeds into.
#define REFLECT_LUA_BEGIN(Component) \
	static void RegisterLuaBindings(sol::state& state, sol::usertype<Lua::ComponentHandle<Component>>& type) { \
		using Self = Component; \
		static const char* s_ReflectComponentName = #Component;

#define REFLECT_LUA_PROPERTY(Member, Description) \
		type[LuaPropertyName(#Member)] = sol::property( \
			Lua::ResolveOnAccess<Self>([](Self& c) -> decltype(c.Member)& { return c.Member; }), \
			Lua::ResolveOnAccess<Self>([](Self& c, const decltype(c.Member)& v) { c.Member = v; }) \
		); \
		RegisterLuaApiEntry({ LuaPropertyName(#Member), Description, LuaApiEntry::Kind::Property, s_ReflectComponentName, LuaTypeName<decltype(Self::Member)>(), true });

#define REFLECT_LUA_PROPERTY_READONLY(Member, Description) \
		type[LuaPropertyName(#Member)] = sol::property( \
			Lua::ResolveOnAccess<Self>([](Self& c) -> const decltype(c.Member)& { return c.Member; }) \
		); \
		RegisterLuaApiEntry({ LuaPropertyName(#Member), Description, LuaApiEntry::Kind::Property, s_ReflectComponentName, LuaTypeName<decltype(Self::Member)>(), true });

#define REFLECT_LUA_PROPERTY_CUSTOM(Name, Description, LuaType, Getter, Setter) \
		type[Name] = sol::property(Lua::ResolveOnAccess<Self>(Getter), Lua::ResolveOnAccess<Self>(Setter)); \
		RegisterLuaApiEntry({ Name, Description, LuaApiEntry::Kind::Property, s_ReflectComponentName, LuaType, true });

// The bound function is moved into sol: given an lvalue, sol keeps a reference to it
#define REFLECT_LUA_FUNCTION(Method, Description) \
		REFLECT_LUA_FUNCTION_PARAMS(Method, nullptr, Description)

// Parameters names the arguments for the docs, e.g. "width, depth, height"
#define REFLECT_LUA_FUNCTION_PARAMS(Method, Parameters, Description) \
		{ \
			auto function = Lua::ResolveOnAccess<Self>(&Self::Method); \
			RegisterLuaApiEntry(Lua::FunctionEntry<Lua::ComponentHandle<Self>, decltype(function)>(#Method, Description, LuaApiEntry::Kind::Function, s_ReflectComponentName, true, Parameters)); \
			type.set_function(#Method, std::move(function)); \
		}

// For functions that need a custom lambda rather than a plain &Self::Method pointer
// (bounds-checked accessors, adapting a different signature, etc).
#define REFLECT_LUA_FUNCTION_CUSTOM(Name, Description, ...) \
		REFLECT_LUA_FUNCTION_CUSTOM_PARAMS(Name, nullptr, Description, __VA_ARGS__)

#define REFLECT_LUA_FUNCTION_CUSTOM_PARAMS(Name, Parameters, Description, ...) \
		{ \
			auto function = Lua::ResolveOnAccess<Self>(__VA_ARGS__); \
			RegisterLuaApiEntry(Lua::FunctionEntry<Lua::ComponentHandle<Self>, decltype(function)>(Name, Description, LuaApiEntry::Kind::Function, s_ReflectComponentName, true, Parameters)); \
			type.set_function(Name, std::move(function)); \
		}

#define REFLECT_LUA_END() \
	}

namespace Lua
{
void BindLogging(sol::state& state);
void BindApp(sol::state& state);
void BindScene(sol::state& state);
void BindEntity(sol::state& state);
void BindInput(sol::state& state);
void BindInputAction(sol::state& state);
void BindMath(sol::state& state);
void BindCommonTypes(sol::state& state);
void BindDebug(sol::state& state);
void BindSignaling(sol::state& state);
void BindPathfinding(sol::state& state);

namespace Detail
{
// What a bound callable returns and takes; Known is false for overload sets and generic lambdas
template<typename F, typename = void>
struct Signature { static constexpr bool Known = false; static constexpr bool IsFunctor = false; using Return = void; using Arguments = TypeList<>; };
template<typename R, typename... Args>
struct Signature<R(*)(Args...)> { static constexpr bool Known = true; static constexpr bool IsFunctor = false; using Return = R; using Arguments = TypeList<Args...>; };
template<typename R, typename... Args>
struct Signature<R(*)(Args...) noexcept> : Signature<R(*)(Args...)> {};
template<typename F>
struct Signature<F, std::enable_if_t<std::is_member_function_pointer_v<F>>> : MemberSignature<F> { static constexpr bool Known = true; static constexpr bool IsFunctor = false; };
template<typename F>
struct Signature<F, std::void_t<decltype(&F::operator())>> : MemberSignature<decltype(&F::operator())> { static constexpr bool Known = true; static constexpr bool IsFunctor = true; };

template<typename T> struct UsertypeSelf { using Type = void; };	// tables and the global state have no self
template<typename T, typename Base> struct UsertypeSelf<sol::basic_usertype<T, Base>> { using Type = T; };

template<typename T>
std::string ParameterTypeName()
{
	if constexpr (sol::meta::is_specialization_of_v<T, sol::optional>)
		return LuaTypeName<typename T::value_type>() + "?";
	else
		return LuaTypeName<T>();
}

// A functor's first argument of the usertype's own type is the object it's called on, not a Lua argument
template<typename Self, bool IsFunctor, typename... Args>
bool ParameterTypeNames(TypeList<Args...>, std::vector<std::string>& names)
{
	bool known = true;
	bool first = true;
	auto add = [&](auto tag)
	{
		using T = std::decay_t<typename decltype(tag)::Type>;
		if constexpr (std::is_same_v<T, sol::this_state>)
			return;
		else if constexpr (std::is_same_v<T, sol::variadic_args>)
			known = false;
		else
		{
			bool isSelf = IsFunctor && std::is_same_v<T, Self> && first;
			first = false;
			if (!isSelf)
				names.push_back(ParameterTypeName<T>());
		}
	};
	(add(TypeTag<Args>{}), ...);
	return known;
}
}

// Argument names for the docs, e.g. Params{ "key, value" }
struct Params
{
	const char* names;
};

// "key, value" -> { "key", "value" }
inline std::vector<std::string> SplitParameterNames(const char* names)
{
	std::vector<std::string> result;
	if (!names)
		return result;
	std::string name;
	for (const char* c = names; ; ++c)
	{
		if (*c == ',' || *c == '\0')
		{
			result.push_back(name);
			name.clear();
			if (*c == '\0')
				break;
		}
		else if (*c != ' ')
			name += *c;
	}
	return result;
}

// An API entry for a bound function, with its return and argument types worked out from F
template<typename Self, typename F>
LuaApiEntry FunctionEntry(const std::string& name, const std::string& description, LuaApiEntry::Kind kind, const std::string& owner, bool isComponent = false, const char* parameterNames = nullptr)
{
	using Signature = Detail::Signature<std::decay_t<F>>;
	LuaApiEntry entry{ name, description, kind, owner, "", isComponent };
	entry.parameterNames = SplitParameterNames(parameterNames);

	if constexpr (!std::is_void_v<typename Signature::Return>)
	{
		std::string returnType = LuaTypeName<typename Signature::Return>();
		entry.type = returnType == "unknown" ? "" : returnType;
	}
	if constexpr (Signature::Known)
		entry.hasSignature = Detail::ParameterTypeNames<Self, Signature::IsFunctor>(typename Signature::Arguments{}, entry.parameters);
	return entry;
}

template<typename T, typename F, typename... Args>
void SetFunction(T& type, const std::string& owner, const std::string& name, Params parameters, const std::string& description, F&& function, Args&&... args)
{
	LuaApiEntry entry = FunctionEntry<typename Detail::UsertypeSelf<T>::Type, F>(name, description, LuaApiEntry::Kind::Function, owner, false, parameters.names);
	type.set_function(name, std::forward<F>(function), std::forward<Args>(args)...);
	RegisterLuaApiEntry(std::move(entry));
}

template<typename T, typename F, typename... Args>
void SetFunction(T& type, const std::string& owner, const std::string& name, const std::string& description, F&& function, Args&&... args)
{
	SetFunction(type, owner, name, Params{ nullptr }, description, std::forward<F>(function), std::forward<Args>(args)...);
}

// Registers a global enum table, e.g. BodyType.Dynamic, and documents each value on the page of documentedOn
inline void SetEnum(sol::state& state, const std::string& name, const std::string& description, std::initializer_list<std::pair<sol::string_view, int>> items, const std::string& documentedOn = "")
{
	state.new_enum(name, items);
	LuaApiEntry entry{ name, description, LuaApiEntry::Kind::Global, "", "" };
	entry.documentedOn = documentedOn;
	RegisterLuaApiEntry(std::move(entry));
	for (const auto& [key, value] : items)
		RegisterLuaApiEntry({ std::string(key), std::to_string(value), LuaApiEntry::Kind::EnumValue, name, "" });
}

template<typename T, typename Property>
void SetProperty(T& type, const std::string& owner, const std::string& name, const std::string& description, const std::string& luaType, Property&& property)
{
	type[name] = std::forward<Property>(property);
	RegisterLuaApiEntry({ name, description, LuaApiEntry::Kind::Property, owner, luaType });
}
}
