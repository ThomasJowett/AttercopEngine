#include "LuaReflection.h"

#include "LuaBindings.h"
#include "LuaManager.h"

#include "Asset/AudioClip.h"
#include "Asset/Font.h"
#include "Asset/LuaScript.h"
#include "Asset/Material.h"
#include "Asset/PhysicsMaterial.h"
#include "Asset/SpriteSheet.h"
#include "Asset/StaticMesh.h"
#include "Asset/Texture.h"
#include "Asset/Tileset.h"
#include "Physics/CollisionLayers.h"
#include "Reflection/Reflection.h"
#include "Scene/AssetManager.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"

#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>

namespace Lua
{
	using Reflection::ComponentType;
	using Reflection::FieldInfo;
	using Reflection::Value;
	using Reflection::ValueType;

	void* ComponentRef::TryGet() const
	{
		if (!scene || !type)
			return nullptr;
		entt::registry& registry = GetSceneRegistry(scene);
		return registry.valid(entity) ? type->tryGet(registry, entity) : nullptr;
	}

	void* ComponentRef::Get() const
	{
		if (void* component = TryGet())
			return component;
		throw std::runtime_error(ScriptLocation(MainLuaState()) + (type ? type->luaName : std::string("Component"))
			+ " is no longer valid: its entity was destroyed or the component was removed");
	}

	namespace
	{
		// How an asset type is passed to and from Lua: as its own usertype, by its C++ class name
		struct AssetKind
		{
			sol::object (*push)(lua_State* state, const Ref<Asset>& asset);
			Ref<Asset> (*pull)(const sol::object& object);
			Ref<Asset> (*load)(const std::filesystem::path& path);
		};

		template<typename A>
		AssetKind MakeAssetKind()
		{
			return {
				[](lua_State* state, const Ref<Asset>& asset) -> sol::object
				{
					Ref<A> typed = std::dynamic_pointer_cast<A>(asset);
					return typed ? sol::make_object(state, typed) : sol::make_object(state, sol::lua_nil);
				},
				[](const sol::object& object) -> Ref<Asset>
				{
					return object.is<Ref<A>>() ? Ref<Asset>(object.as<Ref<A>>()) : nullptr;
				},
				[](const std::filesystem::path& path) -> Ref<Asset>
				{
					if constexpr (std::is_same_v<A, Texture2D>)
						return AssetManager::GetTexture(path);
					else
						return AssetManager::GetAsset<A>(path);
				},
			};
		}

		// Asset fields of other types hold a file path, given to Lua as a string
		const AssetKind* FindAssetKind(const std::string& assetType)
		{
			static const std::unordered_map<std::string, AssetKind> s_Kinds = {
				{ "Texture2D", MakeAssetKind<Texture2D>() },
				{ "Material", MakeAssetKind<Material>() },
				{ "PhysicsMaterial", MakeAssetKind<PhysicsMaterial>() },
				{ "Font", MakeAssetKind<Font>() },
				{ "AudioClip", MakeAssetKind<AudioClip>() },
				{ "Tileset", MakeAssetKind<Tileset>() },
				{ "SpriteSheet", MakeAssetKind<SpriteSheet>() },
				{ "StaticMesh", MakeAssetKind<StaticMesh>() },
				{ "LuaScript", MakeAssetKind<LuaScript>() },
			};
			auto found = s_Kinds.find(assetType);
			return found != s_Kinds.end() ? &found->second : nullptr;
		}

		// The Lua type named in the docs and auto-complete
		std::string LuaTypeOf(const FieldInfo& field, ValueType type)
		{
			switch (type)
			{
			case ValueType::Bool: return "boolean";
			case ValueType::Int:
			case ValueType::UInt: return "integer";
			case ValueType::Float: return "number";
			case ValueType::String: return "string";
			case ValueType::Vec2: return "Vec2";
			case ValueType::Vec3: return "Vec3";
			case ValueType::Colour: return "Colour";
			case ValueType::Enum: return field.enumType ? field.enumType->name : "integer";
			case ValueType::Asset: return FindAssetKind(field.assetType) ? field.assetType : "string";
			case ValueType::Layer: return "string";
			case ValueType::Mask: return "table of string";
			case ValueType::List: return "table of " + LuaTypeOf(field, field.itemType);
			}
			return "unknown";
		}

		[[noreturn]] void RaiseError(lua_State* state, const std::string& message)
		{
			throw std::runtime_error(ScriptLocation(state) + message);
		}

		sol::object ToLua(lua_State* state, const FieldInfo& field, const Value& value)
		{
			switch (value.Type())
			{
			case ValueType::Bool: return sol::make_object(state, value.As<bool>());
			case ValueType::Int: return sol::make_object(state, value.As<int32_t>());
			case ValueType::UInt: return sol::make_object(state, value.As<uint32_t>());
			case ValueType::Float: return sol::make_object(state, value.As<float>());
			case ValueType::String: return sol::make_object(state, value.As<std::string>());
			case ValueType::Vec2: return sol::make_object(state, value.As<Vector2f>());
			case ValueType::Vec3: return sol::make_object(state, value.As<Vector3f>());
			case ValueType::Colour: return sol::make_object(state, value.As<Colour>());
			case ValueType::Enum: return sol::make_object(state, value.As<Reflection::EnumValue>().value);
			case ValueType::Asset:
			{
				const Reflection::AssetValue& asset = value.As<Reflection::AssetValue>();
				const AssetKind* kind = FindAssetKind(field.assetType);
				if (!kind)
					return sol::make_object(state, asset.path.generic_string());
				Ref<Asset> loaded = asset.asset;
				if (!loaded && !asset.path.empty())
					loaded = kind->load(asset.path);
				return kind->push(state, loaded);
			}
			case ValueType::Layer: return sol::make_object(state, Reflection::LayerToText(value.As<Reflection::LayerValue>().bits));
			case ValueType::Mask:
			{
				const uint16_t bits = value.As<Reflection::MaskValue>().bits;
				std::vector<std::string> names;
				for (int i = 0; i < CollisionLayers::MaxLayers; ++i)
				{
					if (bits & (1u << i))
						names.push_back(CollisionLayers::GetDisplayName(i));
				}
				return sol::make_object(state, sol::as_table(names));
			}
			case ValueType::List:
			{
				sol::state_view view(state);
				sol::table items = view.create_table();
				int index = 1;
				for (const Value& item : value.As<Reflection::ValueList>())
					items[index++] = ToLua(state, field, item);
				return items;
			}
			}
			return sol::make_object(state, sol::lua_nil);
		}

		// nullopt if the Lua value doesn't fit the field
		std::optional<Value> FromLua(lua_State* state, const FieldInfo& field, ValueType type, const sol::object& object)
		{
			const sol::type luaType = object.get_type();
			switch (type)
			{
			case ValueType::Bool:
				if (luaType == sol::type::boolean)
					return Value(object.as<bool>());
				break;
			case ValueType::Int:
				if (luaType == sol::type::number)
					return Value((int32_t)object.as<double>());
				break;
			case ValueType::UInt:
				if (luaType == sol::type::number && object.as<double>() >= 0.0)
					return Value((uint32_t)object.as<double>());
				break;
			case ValueType::Float:
				if (luaType == sol::type::number)
					return Value(object.as<float>());
				break;
			case ValueType::String:
				if (luaType == sol::type::string)
					return Value(object.as<std::string>());
				break;
			case ValueType::Vec2:
				if (object.is<Vector2f>())
					return Value(object.as<Vector2f>());
				break;
			case ValueType::Vec3:
				if (object.is<Vector3f>())
					return Value(object.as<Vector3f>());
				break;
			case ValueType::Colour:
				if (object.is<Colour>())
					return Value(object.as<Colour>());
				break;
			case ValueType::Enum:
				if (luaType == sol::type::number)
					return Value(Reflection::EnumValue{ field.enumType, (int32_t)object.as<double>() });
				if (luaType == sol::type::string && field.enumType)
				{
					if (std::optional<int32_t> known = field.enumType->ValueOf(object.as<std::string>()))
						return Value(Reflection::EnumValue{ field.enumType, *known });
					RaiseError(state, "\"" + object.as<std::string>() + "\" isn't a " + field.enumType->name);
				}
				break;
			case ValueType::Asset:
			{
				if (luaType == sol::type::lua_nil)
					return Value(Reflection::AssetValue{});
				if (luaType == sol::type::string)
					return Value(Reflection::AssetValue{ object.as<std::string>(), nullptr });
				if (const AssetKind* kind = FindAssetKind(field.assetType))
				{
					if (Ref<Asset> asset = kind->pull(object))
						return Value(Reflection::AssetValue{ asset->GetFilepath(), asset });
				}
				break;
			}
			case ValueType::Layer:
				if (luaType == sol::type::string)
				{
					bool known = false;
					uint16_t bits = Reflection::LayerFromText(object.as<std::string>(), &known);
					if (!known)
						RaiseError(state, "There's no collision layer called \"" + object.as<std::string>() + "\"");
					return Value(Reflection::LayerValue{ bits });
				}
				break;
			case ValueType::Mask:
			{
				std::string text;
				if (luaType == sol::type::string)
					text = object.as<std::string>();
				else if (luaType == sol::type::table)
				{
					for (const auto& [key, name] : object.as<sol::table>())
					{
						if (name.get_type() != sol::type::string)
							return std::nullopt;
						text += (text.empty() ? "" : ", ") + name.as<std::string>();
					}
					if (text.empty())
						text = "Nothing";
				}
				else
					break;
				std::vector<std::string> unknown;
				uint16_t bits = Reflection::MaskFromText(text, &unknown);
				if (!unknown.empty())
					RaiseError(state, "There's no collision layer called \"" + unknown.front() + "\"");
				return Value(Reflection::MaskValue{ bits });
			}
			case ValueType::List:
				if (luaType == sol::type::table)
				{
					Reflection::ValueList items;
					sol::table table = object.as<sol::table>();
					for (size_t i = 1; i <= table.size(); ++i)
					{
						std::optional<Value> item = FromLua(state, field, field.itemType, table[i]);
						if (!item)
							return std::nullopt;
						items.push_back(std::move(*item));
					}
					return Value(std::move(items));
				}
				break;
			}
			return std::nullopt;
		}

		// What the Component usertype finds for each component's keys
		struct TypeBinding
		{
			std::unordered_map<std::string, int> fields;
			sol::table functions;
			sol::table properties;
		};

		using Bindings = std::unordered_map<const ComponentType*, TypeBinding>;

		// A read-only global table of an enum's names, as sol's new_enum makes
		void SetEnum(sol::state& state, const Reflection::EnumType& type, const std::string& documentedOn)
		{
			sol::table values = state.create_table();
			for (const Reflection::EnumType::Entry& entry : type.entries)
				values[entry.name] = entry.value;
			sol::table metatable = state.create_table();
			metatable[sol::meta_function::index] = values;
			metatable[sol::meta_function::new_index] = [name = type.name](sol::this_state luaState)
				{
					RaiseError(luaState, name + " can't be changed");
				};
			sol::table table = state.create_named_table(type.name);
			table[sol::metatable_key] = metatable;

			LuaApiEntry entry{ type.name, type.description, LuaApiEntry::Kind::Global, "", "" };
			entry.documentedOn = documentedOn;
			RegisterLuaApiEntry(std::move(entry));
			for (const Reflection::EnumType::Entry& value : type.entries)
				RegisterLuaApiEntry({ value.name, std::to_string(value.value), LuaApiEntry::Kind::EnumValue, type.name, "" });
		}

		// The first bound component with a field of this enum, where its table is documented
		std::string DocumentedOn(const Reflection::EnumType& type)
		{
			for (const ComponentType* component : Reflection::Components())
			{
				if (component->IsInternal())
					continue;
				for (const FieldInfo& field : component->fields)
				{
					if (field.enumType == &type && !field.Has(Reflection::FieldFlags::NoLua))
						return component->luaName;
				}
			}
			return {};
		}

		bool RemovesAfterUpdate(const ComponentType* type)
		{
			return type == &Reflection::TypeOf<LuaScriptComponent>() || type == &Reflection::TypeOf<BehaviourTreeComponent>()
				|| type == &Reflection::TypeOf<StateMachineComponent>();
		}

		void CheckNotExcluded(sol::this_state state, Entity& entity, const ComponentType& type)
		{
			if (const ComponentType* other = Reflection::ExcludedBy(entity.GetScene()->GetRegistry(), entity.GetHandle(), type))
				RaiseError(state, entity.GetName() + " has a " + other->luaName + ", which can't be on the same entity as a " + type.luaName);
		}

		void BindAccessors(sol::usertype<Entity>& entityType, const ComponentType* type)
		{
			const std::string& name = type->luaName;
			auto registerAccessor = [&](const std::string& functionName, const std::string& description, auto&& function, bool returnsComponent)
				{
					LuaApiEntry entry = FunctionEntry<Entity, decltype(function)>(functionName, description, LuaApiEntry::Kind::ComponentAccessor, name, true);
					if (returnsComponent)
						entry.type = name;
					entityType.set_function(functionName, std::forward<decltype(function)>(function));
					RegisterLuaApiEntry(std::move(entry));
				};

			registerAccessor("Add" + name, "Add a " + name + " to this entity; errors if it already has one, or has a component it can't be beside", [type, function = "Add" + name](sol::this_state state, Entity& entity)
				{
					CheckEntityValid(entity, function.c_str(), state);
					entt::registry& registry = entity.GetScene()->GetRegistry();
					if (type->tryGet(registry, entity.GetHandle()))
						RaiseError(state, entity.GetName() + " already has a " + type->luaName + "; use GetOrAdd" + type->luaName + " instead");
					CheckNotExcluded(state, entity, *type);
					type->emplace(registry, entity.GetHandle());
					entity.GetScene()->MakeDirty();
					return ComponentRef{ type, entity.GetHandle(), entity.GetScene() };
				}, true);
			registerAccessor("Remove" + name, "Remove the " + name + " from this entity; errors if it doesn't have one", [type, function = "Remove" + name](sol::this_state state, Entity& entity)
				{
					CheckEntityValid(entity, function.c_str(), state);
					if (!type->tryGet(entity.GetScene()->GetRegistry(), entity.GetHandle()))
						RaiseError(state, entity.GetName() + " has no " + type->luaName + " to remove");
					// The scene may be running this very script, tree or state machine, so wait until its update has finished
					if (RemovesAfterUpdate(type))
					{
						entity.GetScene()->DeferUntilUpdateEnds([type, entity]() mutable
							{
								if (entity.IsSceneValid() && type->tryGet(entity.GetScene()->GetRegistry(), entity.GetHandle()))
								{
									type->remove(entity.GetScene()->GetRegistry(), entity.GetHandle());
									entity.GetScene()->MakeDirty();
								}
							});
						return;
					}
					type->remove(entity.GetScene()->GetRegistry(), entity.GetHandle());
					entity.GetScene()->MakeDirty();
				}, false);
			registerAccessor("Has" + name, "Check whether this entity has a " + name, [type, function = "Has" + name](sol::this_state state, Entity& entity)
				{
					CheckEntityValid(entity, function.c_str(), state);
					return type->tryGet(entity.GetScene()->GetRegistry(), entity.GetHandle()) != nullptr;
				}, false);
			registerAccessor("GetOrAdd" + name, "Get the entity's " + name + ", adding one first if it doesn't already have one", [type, function = "GetOrAdd" + name](sol::this_state state, Entity& entity)
				{
					CheckEntityValid(entity, function.c_str(), state);
					entt::registry& registry = entity.GetScene()->GetRegistry();
					if (!type->tryGet(registry, entity.GetHandle()))
					{
						CheckNotExcluded(state, entity, *type);
						type->emplace(registry, entity.GetHandle());
						entity.GetScene()->MakeDirty();
					}
					return ComponentRef{ type, entity.GetHandle(), entity.GetScene() };
				}, true);
			registerAccessor("Get" + name, "Get the entity's " + name + ", or nil if it doesn't have one", [type, function = "Get" + name](sol::this_state state, Entity& entity) -> sol::optional<ComponentRef>
				{
					CheckEntityValid(entity, function.c_str(), state);
					if (!type->tryGet(entity.GetScene()->GetRegistry(), entity.GetHandle()))
						return sol::nullopt;
					return ComponentRef{ type, entity.GetHandle(), entity.GetScene() };
				}, true);
		}
	}

	void BindComponents(sol::state& state)
	{
		PROFILE_FUNCTION();
		RegisterComponents();

		auto bindings = std::make_shared<Bindings>();

		sol::usertype<ComponentRef> componentType = state.new_usertype<ComponentRef>("Component", sol::no_constructor,
			sol::meta_function::equal_to, [](const ComponentRef& a, const ComponentRef& b) { return a == b; },
			sol::meta_function::index, [bindings](sol::this_state luaState, const ComponentRef& ref, const std::string& key) -> sol::object
			{
				auto found = bindings->find(ref.type);
				if (found == bindings->end())
					return sol::make_object(luaState, sol::lua_nil);
				const TypeBinding& binding = found->second;
				if (auto field = binding.fields.find(key); field != binding.fields.end())
				{
					const FieldInfo& info = ref.type->fields[field->second];
					return ToLua(luaState, info, info.get(ref.Get()));
				}
				if (sol::optional<sol::table> property = binding.properties.get<sol::optional<sol::table>>(key))
				{
					sol::function get = (*property)["get"];
					return get(ref);
				}
				return binding.functions.get<sol::object>(key);
			},
			sol::meta_function::new_index, [bindings](sol::this_state luaState, const ComponentRef& ref, const std::string& key, const sol::object& value)
			{
				auto found = bindings->find(ref.type);
				const std::string name = (ref.type ? ref.type->luaName : std::string("Component")) + "." + key;
				if (found == bindings->end())
					RaiseError(luaState, name + " isn't a property");
				const TypeBinding& binding = found->second;
				if (auto field = binding.fields.find(key); field != binding.fields.end())
				{
					const FieldInfo& info = ref.type->fields[field->second];
					if (info.Has(Reflection::FieldFlags::ReadOnly))
						RaiseError(luaState, name + " is read-only");
					std::optional<Value> converted = FromLua(luaState, info, info.type, value);
					if (!converted)
						RaiseError(luaState, name + " can't be set to a " + std::string(sol::type_name(luaState, value.get_type())) + "; it's a " + LuaTypeOf(info, info.type));
					ref.Get();
					if (!Reflection::Set(Entity(ref.entity, ref.scene), Reflection::PropertyPath{ ref.type, field->second }, *converted, Reflection::ChangeSource::Lua))
						RaiseError(luaState, name + " can't be set to " + converted->ToString());
					return;
				}
				if (sol::optional<sol::table> property = binding.properties.get<sol::optional<sol::table>>(key))
				{
					sol::function set = (*property)["set"];
					set(ref, value);
					return;
				}
				RaiseError(luaState, name + " isn't a property");
			});
		(void)componentType;

		for (const Reflection::EnumType* type : Reflection::Enums())
			SetEnum(state, *type, DocumentedOn(*type));

		sol::usertype<Entity> entityType = state["Entity"].get_or_create<sol::usertype<Entity>>();
		for (const ComponentType* type : Reflection::Components())
		{
			if (type->IsInternal())
				continue;

			TypeBinding& binding = (*bindings)[type];
			binding.functions = state.create_table();
			binding.properties = state.create_table();

			binding.functions.set_function("IsValid", [](const ComponentRef& ref) { return ref.TryGet() != nullptr; });
			RegisterLuaApiEntry({ "IsValid", "Whether this still refers to a live component; false once its entity is destroyed or the component removed",
				LuaApiEntry::Kind::Function, type->luaName, "", true });

			for (size_t i = 0; i < type->fields.size(); ++i)
			{
				const FieldInfo& field = type->fields[i];
				if (field.Has(Reflection::FieldFlags::NoLua) || field.IsCustom())
					continue;
				binding.fields.emplace(field.name, (int)i);
				const std::string description = field.Has(Reflection::FieldFlags::Degrees) ? field.description + ", in radians" : field.description;
				RegisterLuaApiEntry({ field.name, description, LuaApiEntry::Kind::Property, type->luaName, LuaTypeOf(field, field.type), true });
			}

			if (type->luaFunctions)
			{
				ComponentFunctions functions(binding.functions, binding.properties);
				reinterpret_cast<void(*)(ComponentFunctions&)>(type->luaFunctions)(functions);
			}

			BindAccessors(entityType, type);
		}
	}
}
