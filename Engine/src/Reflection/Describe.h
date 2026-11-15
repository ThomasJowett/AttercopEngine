#pragma once

// The builder components use to describe themselves; include it only from .cpp files

#include "Reflection/Reflection.h"
#include "Scene/AssetManager.h"
#include "Scene/Entity.h"
#include "Utilities/StringUtils.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace tinyxml2
{
	class XMLElement;
}

namespace Reflection
{
	namespace Detail
	{
		// The value type of each C++ field type, and conversions to and from Value
		template<typename V, typename = void>
		struct Convert
		{
			static_assert(sizeof(V) == 0, "This field type has no Reflection value type");
		};

		template<typename V, ValueType Type>
		struct ConvertSame
		{
			static constexpr ValueType type = Type;
			static Value To(const V& value) { return Value(value); }
			static bool From(const Value& value, V& out)
			{
				if (const V* stored = value.TryAs<V>())
				{
					out = *stored;
					return true;
				}
				return false;
			}
		};

		template<> struct Convert<bool> : ConvertSame<bool, ValueType::Bool> {};
		template<> struct Convert<int32_t> : ConvertSame<int32_t, ValueType::Int> {};
		template<> struct Convert<uint32_t> : ConvertSame<uint32_t, ValueType::UInt> {};
		template<> struct Convert<float> : ConvertSame<float, ValueType::Float> {};
		template<> struct Convert<std::string> : ConvertSame<std::string, ValueType::String> {};
		template<> struct Convert<Vector2f> : ConvertSame<Vector2f, ValueType::Vec2> {};
		template<> struct Convert<Vector3f> : ConvertSame<Vector3f, ValueType::Vec3> {};
		template<> struct Convert<Colour> : ConvertSame<Colour, ValueType::Colour> {};

		template<typename E>
		struct Convert<E, std::enable_if_t<std::is_enum_v<E>>>
		{
			static constexpr ValueType type = ValueType::Enum;
			static Value To(E value) { return EnumValue{ &EnumOf<E>(), (int32_t)value }; }
			static bool From(const Value& value, E& out)
			{
				const EnumValue* stored = value.TryAs<EnumValue>();
				if (!stored || stored->type != &EnumOf<E>() || !stored->type->NameOf(stored->value))
					return false;
				out = (E)stored->value;
				return true;
			}
		};

		template<typename A>
		Ref<A> LoadAsset(const std::filesystem::path& path)
		{
			if constexpr (std::is_same_v<A, Texture2D>)
				return AssetManager::GetTexture(path);
			else
				return AssetManager::GetAsset<A>(path);
		}

		template<typename A>
		std::string TypeName()
		{
			std::string name(type_name<A>());
			return name.substr(0, name.find('\n'));
		}

		template<typename A>
		struct Convert<Ref<A>>
		{
			static constexpr ValueType type = ValueType::Asset;
			static Value To(const Ref<A>& asset)
			{
				return AssetValue{ asset ? asset->GetFilepath() : std::filesystem::path(), asset };
			}
			// A held asset of another type doesn't fit; a path is loaded
			static bool From(const Value& value, Ref<A>& out)
			{
				const AssetValue* stored = value.TryAs<AssetValue>();
				if (!stored)
					return false;
				if (stored->asset)
				{
					Ref<A> asset = std::dynamic_pointer_cast<A>(stored->asset);
					if (!asset)
						return false;
					out = asset;
				}
				else
				{
					out = stored->path.empty() ? nullptr : LoadAsset<A>(stored->path);
				}
				return true;
			}
		};

		// A file the component loads itself, held by its path
		template<>
		struct Convert<std::filesystem::path>
		{
			static constexpr ValueType type = ValueType::Asset;
			static Value To(const std::filesystem::path& path) { return AssetValue{ path, nullptr }; }
			static bool From(const Value& value, std::filesystem::path& out)
			{
				const AssetValue* stored = value.TryAs<AssetValue>();
				if (!stored)
					return false;
				out = stored->asset ? stored->asset->GetFilepath() : stored->path;
				return true;
			}
		};

		// For properties whose getter and setter handle the asset themselves
		template<>
		struct Convert<AssetValue> : ConvertSame<AssetValue, ValueType::Asset> {};

		template<typename V>
		struct Convert<std::vector<V>>
		{
			static constexpr ValueType type = ValueType::List;
			static Value To(const std::vector<V>& list)
			{
				ValueList items;
				items.reserve(list.size());
				for (const V& item : list)
					items.push_back(Convert<V>::To(item));
				return items;
			}
			static bool From(const Value& value, std::vector<V>& out)
			{
				const ValueList* stored = value.TryAs<ValueList>();
				if (!stored)
					return false;
				std::vector<V> list(stored->size());
				for (size_t i = 0; i < stored->size(); ++i)
				{
					if (!Convert<V>::From((*stored)[i], list[i]))
						return false;
				}
				out = std::move(list);
				return true;
			}
		};

		// Collision layer and mask fields are uint16_t bits
		struct ConvertLayer
		{
			static Value To(uint16_t bits) { return LayerValue{ bits }; }
			static bool From(const Value& value, uint16_t& out)
			{
				const LayerValue* stored = value.TryAs<LayerValue>();
				// Exactly one bit
				if (!stored || stored->bits == 0 || (stored->bits & (stored->bits - 1)) != 0)
					return false;
				out = stored->bits;
				return true;
			}
		};

		struct ConvertMask
		{
			static Value To(uint16_t bits) { return MaskValue{ bits }; }
			static bool From(const Value& value, uint16_t& out)
			{
				const MaskValue* stored = value.TryAs<MaskValue>();
				if (!stored)
					return false;
				out = stored->bits;
				return true;
			}
		};

		template<typename V>
		struct IsList : std::false_type {};
		template<typename V>
		struct IsList<std::vector<V>> : std::true_type { using Item = V; };

		template<typename P>
		struct MemberPointer;
		template<typename C, typename M>
		struct MemberPointer<M C::*> { using Type = M; };

		template<typename T, auto Member, typename Conversion>
		Value GetMember(const void* component)
		{
			return Conversion::To(static_cast<const T*>(component)->*Member);
		}

		template<typename T, auto Member, typename Conversion>
		bool SetMember(void* component, const Value& value)
		{
			typename MemberPointer<decltype(Member)>::Type converted{};
			if (!Conversion::From(value, converted))
				return false;
			static_cast<T*>(component)->*Member = std::move(converted);
			return true;
		}

		template<typename T, auto Getter>
		using PropertyType = std::decay_t<std::invoke_result_t<decltype(Getter), const T&>>;

		template<typename T, auto Getter>
		Value GetProperty(const void* component)
		{
			return Convert<PropertyType<T, Getter>>::To(std::invoke(Getter, *static_cast<const T*>(component)));
		}

		template<typename T, auto Setter, typename V>
		bool SetProperty(void* component, const Value& value)
		{
			V converted{};
			if (!Convert<V>::From(value, converted))
				return false;
			// A setter can return false to refuse the value
			if constexpr (std::is_same_v<std::invoke_result_t<decltype(Setter), T&, const V&>, bool>)
				return std::invoke(Setter, *static_cast<T*>(component), converted);
			else
			{
				std::invoke(Setter, *static_cast<T*>(component), converted);
				return true;
			}
		}

		template<typename T, auto Save>
		void SaveCustom(const void* component, tinyxml2::XMLElement* element)
		{
			Save(*static_cast<const T*>(component), element);
		}

		template<typename T, auto Load>
		void LoadCustom(void* component, const tinyxml2::XMLElement* element)
		{
			Load(*static_cast<T*>(component), element);
		}

		template<typename T>
		bool CallPredicate(ErasedFunction function, const void* component)
		{
			return reinterpret_cast<bool(*)(const T&)>(function)(*static_cast<const T*>(component));
		}

		template<typename T>
		void CallHook(ErasedFunction function, Entity entity, void* component)
		{
			reinterpret_cast<void(*)(Entity, T&)>(function)(entity, *static_cast<T*>(component));
		}

		// "TilingFactor" to "Tiling Factor"
		std::string SplitWords(const std::string& name);

		// Fills in the type details of a field from its C++ type
		template<typename V>
		void SetFieldType(FieldInfo& field)
		{
			field.type = Convert<V>::type;
			if constexpr (std::is_enum_v<V>)
				field.enumType = &EnumOf<V>();
			else if constexpr (std::is_same_v<V, std::filesystem::path> || std::is_same_v<V, AssetValue>)
				field.assetType = "File";
			else if constexpr (Convert<V>::type == ValueType::Asset)
				field.assetType = TypeName<typename V::element_type>();
			else if constexpr (IsList<V>::value)
			{
				using Item = typename IsList<V>::Item;
				static_assert(!IsList<Item>::value, "A list of lists has no Reflection value type");
				SetFieldType<Item>(field);
				field.itemType = field.type;
				field.type = ValueType::List;
			}
		}
	}

	template<typename T>
	class FieldBuilder
	{
	public:
		FieldBuilder(ComponentType& type, size_t index)
			:m_Type(type), m_Index(index) {}

		// Inspector label, when splitting the name into words isn't right
		FieldBuilder& Label(const char* label) { Field().label = label; return *this; }
		// Limits and step for the inspector's drag widgets; not enforced on Lua or files
		FieldBuilder& Range(float min, float max) { Field().hasRange = true; Field().min = min; Field().max = max; return *this; }
		FieldBuilder& Speed(float speed) { Field().speed = speed; return *this; }
		// Shown in the inspector only when the predicate holds; still saved
		FieldBuilder& ShowIf(bool(*predicate)(const T&))
		{
			Field().showIf = { reinterpret_cast<ErasedFunction>(predicate), &Detail::CallPredicate<T> };
			return *this;
		}
		// Saved inside a child element and shown under a heading, only when the predicate holds. The field's
		// name becomes the group's name followed by its own ("OuterRadius" in "Torus" is "TorusOuterRadius")
		FieldBuilder& Group(const char* name, bool(*when)(const T&));
		// Written as the component element's own Filepath
		FieldBuilder& MainAsset() { Field().flags |= FieldFlags::MainAsset; return *this; }
		// A string saved as the element's text
		FieldBuilder& AsText() { Field().flags |= FieldFlags::AsText; return *this; }
		// For a list, the element name of each item
		FieldBuilder& Item(const char* name) { Field().itemName = name; return *this; }
		// For a file held by its path, the kind of file
		FieldBuilder& AssetType(const char* name) { Field().assetType = name; return *this; }
		FieldBuilder& ReadOnly() { Field().flags |= FieldFlags::ReadOnly; return *this; }
		// Shown and scriptable, but not written to files or the snapshot
		FieldBuilder& NotSaved() { Field().flags |= FieldFlags::NotSaved; return *this; }
		FieldBuilder& NoLua() { Field().flags |= FieldFlags::NoLua; return *this; }
		FieldBuilder& HideInInspector() { Field().flags |= FieldFlags::HideInInspector; return *this; }
		// Runs after the field changes through Reflection::Set or Restore
		FieldBuilder& OnChanged(void(*hook)(Entity, T&))
		{
			Field().onChanged = { reinterpret_cast<ErasedFunction>(hook), &Detail::CallHook<T> };
			return *this;
		}

	private:
		FieldInfo& Field() { return m_Type.fields[m_Index]; }

		ComponentType& m_Type;
		size_t m_Index;
	};

	template<typename T>
	class Builder
	{
	public:
		explicit Builder(ComponentType& type)
			:m_Type(type) {}

		// The XML element name
		Builder& Name(const char* name) { m_Type.name = name; return *this; }
		// For the Add Component menu
		Builder& Category(const char* category) { m_Type.category = category; return *this; }
		Builder& Description(const char* description) { m_Type.description = description; return *this; }
		// Shown without a delete option
		Builder& Required() { m_Type.flags |= ComponentFlags::Required; return *this; }
		// Not in the Add Component menu, the inspector or Lua's generic accessors
		Builder& Internal() { m_Type.flags |= ComponentFlags::Internal; return *this; }
		// Runs after any field changes through Reflection::Set, and once after Restore
		Builder& OnChanged(void(*hook)(Entity, T&))
		{
			m_Type.onChanged = { reinterpret_cast<ErasedFunction>(hook), &Detail::CallHook<T> };
			return *this;
		}
		// Registers the component's Lua functions; the Lua bindings call it with the component's usertype
		template<typename Function>
		Builder& LuaFunctions(Function function)
		{
			static_assert(std::is_pointer_v<Function>, "LuaFunctions takes a function pointer");
			m_Type.luaFunctions = reinterpret_cast<ErasedFunction>(function);
			return *this;
		}

		// A data member: Field<&SpriteComponent::tint>("Tint", "...")
		template<auto Member>
		FieldBuilder<T> Field(const char* name, const char* description)
		{
			using V = typename Detail::MemberPointer<decltype(Member)>::Type;
			FieldInfo& field = AddField(name, description);
			Detail::SetFieldType<V>(field);
			field.get = &Detail::GetMember<T, Member, Detail::Convert<V>>;
			field.set = &Detail::SetMember<T, Member, Detail::Convert<V>>;
			return { m_Type, m_Type.fields.size() - 1 };
		}

		// A getter and setter: member functions, or functions taking the component first
		template<auto Getter, auto Setter>
		FieldBuilder<T> Property(const char* name, const char* description)
		{
			using V = Detail::PropertyType<T, Getter>;
			FieldInfo& field = AddField(name, description);
			Detail::SetFieldType<V>(field);
			field.get = &Detail::GetProperty<T, Getter>;
			field.set = &Detail::SetProperty<T, Setter, V>;
			return { m_Type, m_Type.fields.size() - 1 };
		}

		// A getter alone, which makes the field read-only
		template<auto Getter>
		FieldBuilder<T> Property(const char* name, const char* description)
		{
			using V = Detail::PropertyType<T, Getter>;
			FieldInfo& field = AddField(name, description);
			Detail::SetFieldType<V>(field);
			field.flags |= FieldFlags::ReadOnly;
			field.get = &Detail::GetProperty<T, Getter>;
			return { m_Type, m_Type.fields.size() - 1 };
		}

		// A uint16_t collision layer bit
		template<auto Member>
		FieldBuilder<T> Layer(const char* name, const char* description)
		{
			static_assert(std::is_same_v<typename Detail::MemberPointer<decltype(Member)>::Type, uint16_t>, "A layer is a uint16_t");
			FieldInfo& field = AddField(name, description);
			field.type = ValueType::Layer;
			field.get = &Detail::GetMember<T, Member, Detail::ConvertLayer>;
			field.set = &Detail::SetMember<T, Member, Detail::ConvertLayer>;
			return { m_Type, m_Type.fields.size() - 1 };
		}

		// A uint16_t collision mask
		template<auto Member>
		FieldBuilder<T> Mask(const char* name, const char* description)
		{
			static_assert(std::is_same_v<typename Detail::MemberPointer<decltype(Member)>::Type, uint16_t>, "A mask is a uint16_t");
			FieldInfo& field = AddField(name, description);
			field.type = ValueType::Mask;
			field.get = &Detail::GetMember<T, Member, Detail::ConvertMask>;
			field.set = &Detail::SetMember<T, Member, Detail::ConvertMask>;
			return { m_Type, m_Type.fields.size() - 1 };
		}

		// Saved and loaded by hand, for the few fields no value type fits. Save is void(const T&, XMLElement*) and
		// Load is void(T&, const XMLElement*), both given the component's element
		template<auto Save, auto Load>
		FieldBuilder<T> Custom(const char* name, const char* description)
		{
			FieldInfo& field = AddField(name, description);
			field.flags |= FieldFlags::Custom;
			field.save = &Detail::SaveCustom<T, Save>;
			field.load = &Detail::LoadCustom<T, Load>;
			return { m_Type, m_Type.fields.size() - 1 };
		}

	private:
		FieldInfo& AddField(const char* name, const char* description)
		{
			CORE_ASSERT(m_Type.FindField(name) < 0, "Two fields with the same name");
			FieldInfo& field = m_Type.fields.emplace_back();
			field.name = name;
			field.xmlName = name;
			field.label = Detail::SplitWords(name);
			field.description = description;
			return field;
		}

		ComponentType& m_Type;
	};

	template<typename T>
	FieldBuilder<T>& FieldBuilder<T>::Group(const char* name, bool(*when)(const T&))
	{
		FieldInfo& field = Field();
		int group = -1;
		for (size_t i = 0; i < m_Type.groups.size(); ++i)
		{
			if (m_Type.groups[i].name == name)
				group = (int)i;
		}
		if (group < 0)
		{
			m_Type.groups.push_back({ name, { reinterpret_cast<ErasedFunction>(when), &Detail::CallPredicate<T> } });
			group = (int)m_Type.groups.size() - 1;
		}
		field.group = group;
		field.name = std::string(name) + field.xmlName;
		CORE_ASSERT(m_Type.FindField(field.name) == (int)m_Index, "Two fields with the same name");
		return *this;
	}

	// Registers a component from its static Describe member, once
	template<typename T>
	void Register()
	{
		if (IsRegistered<T>())
			return;

		ComponentType& type = Detail::AddComponent();
		type.luaName = Detail::TypeName<T>();
		type.name = type.luaName.substr(0, type.luaName.rfind("Component"));
		type.tryGet = [](entt::registry& registry, entt::entity entity) -> void* { return registry.try_get<T>(entity); };
		type.emplace = [](entt::registry& registry, entt::entity entity) -> void* { return &registry.emplace<T>(entity); };
		type.remove = [](entt::registry& registry, entt::entity entity) { registry.remove<T>(entity); };
		type.create = []() -> std::shared_ptr<void> { return std::make_shared<T>(); };

		Builder<T> builder(type);
		T::Describe(builder);
		Detail::s_ComponentType<T> = &type;
	}
}
