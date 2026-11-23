#include "Reflection/Reflection.h"
#include "Reflection/Describe.h"

#include "Physics/CollisionLayers.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Logging/Logger.h"

#include "TinyXml2/tinyxml2.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>

namespace Reflection
{
	const char* ValueTypeName(ValueType type)
	{
		switch (type)
		{
		case ValueType::Bool: return "Bool";
		case ValueType::Int: return "Int";
		case ValueType::UInt: return "UInt";
		case ValueType::Float: return "Float";
		case ValueType::String: return "String";
		case ValueType::Vec2: return "Vec2";
		case ValueType::Vec3: return "Vec3";
		case ValueType::Quaternion: return "Quaternion";
		case ValueType::Colour: return "Colour";
		case ValueType::Enum: return "Enum";
		case ValueType::Asset: return "Asset";
		case ValueType::Layer: return "Layer";
		case ValueType::Mask: return "Mask";
		case ValueType::List: return "List";
		}
		return "Unknown";
	}

	int PartCount(ValueType type)
	{
		switch (type)
		{
		case ValueType::Vec2: return 2;
		case ValueType::Vec3: return 3;
		case ValueType::Colour: return 4;
		default: return 0;
		}
	}

	const char* PartName(ValueType type, int part)
	{
		static const char* const vectorParts[] = { "X", "Y", "Z" };
		static const char* const colourParts[] = { "R", "G", "B", "A" };
		if (part < 0 || part >= PartCount(type))
			return nullptr;
		return type == ValueType::Colour ? colourParts[part] : vectorParts[part];
	}

	const char* EnumType::NameOf(int32_t value) const
	{
		for (const Entry& entry : entries)
		{
			if (entry.value == value)
				return entry.name.c_str();
		}
		return nullptr;
	}

	std::optional<int32_t> EnumType::ValueOf(std::string_view entryName) const
	{
		for (const Entry& entry : entries)
		{
			if (entry.name == entryName)
				return entry.value;
		}
		return std::nullopt;
	}

	std::optional<float> Value::GetPart(int part) const
	{
		if (const Vector2f* vector2 = TryAs<Vector2f>(); vector2 && part >= 0 && part < 2)
			return part == 0 ? vector2->x : vector2->y;
		if (const Vector3f* vector3 = TryAs<Vector3f>(); vector3 && part >= 0 && part < 3)
			return part == 0 ? vector3->x : part == 1 ? vector3->y : vector3->z;
		if (const Colour* colour = TryAs<Colour>(); colour && part >= 0 && part < 4)
			return part == 0 ? colour->r : part == 1 ? colour->g : part == 2 ? colour->b : colour->a;
		return std::nullopt;
	}

	bool Value::SetPart(int part, float value)
	{
		if (Vector2f* vector2 = std::get_if<Vector2f>(&m_Data); vector2 && part >= 0 && part < 2)
		{
			(part == 0 ? vector2->x : vector2->y) = value;
			return true;
		}
		if (Vector3f* vector3 = std::get_if<Vector3f>(&m_Data); vector3 && part >= 0 && part < 3)
		{
			(part == 0 ? vector3->x : part == 1 ? vector3->y : vector3->z) = value;
			return true;
		}
		if (Colour* colour = std::get_if<Colour>(&m_Data); colour && part >= 0 && part < 4)
		{
			(part == 0 ? colour->r : part == 1 ? colour->g : part == 2 ? colour->b : colour->a) = value;
			return true;
		}
		return false;
	}

	std::string Value::ToString() const
	{
		std::ostringstream text;
		switch (Type())
		{
		case ValueType::Bool: text << (As<bool>() ? "true" : "false"); break;
		case ValueType::Int: text << As<int32_t>(); break;
		case ValueType::UInt: text << As<uint32_t>(); break;
		case ValueType::Float: text << As<float>(); break;
		case ValueType::String: text << '"' << As<std::string>() << '"'; break;
		case ValueType::Vec2:
		case ValueType::Vec3:
		case ValueType::Colour:
		{
			text << '(';
			for (int part = 0; part < PartCount(Type()); ++part)
				text << (part > 0 ? ", " : "") << *GetPart(part);
			text << ')';
			break;
		}
		case ValueType::Quaternion:
		{
			const Quaternion& q = As<Quaternion>();
			text << "(w " << q.w << ", x " << q.x << ", y " << q.y << ", z " << q.z << ')';
			break;
		}
		case ValueType::Enum:
		{
			const EnumValue& value = As<EnumValue>();
			const char* name = value.type ? value.type->NameOf(value.value) : nullptr;
			if (name)
				text << name;
			else
				text << value.value;
			break;
		}
		case ValueType::Asset: text << "Asset(" << As<AssetValue>().path.generic_string() << ')'; break;
		case ValueType::Layer: text << LayerToText(As<LayerValue>().bits); break;
		case ValueType::Mask: text << '[' << MaskToText(As<MaskValue>().bits) << ']'; break;
		case ValueType::List:
		{
			text << '{';
			const ValueList& items = As<ValueList>();
			for (size_t i = 0; i < items.size(); ++i)
				text << (i > 0 ? ", " : "") << items[i].ToString();
			text << '}';
			break;
		}
		}
		return text.str();
	}

	static std::string_view Trim(std::string_view text)
	{
		while (!text.empty() && std::isspace((unsigned char)text.front()))
			text.remove_prefix(1);
		while (!text.empty() && std::isspace((unsigned char)text.back()))
			text.remove_suffix(1);
		return text;
	}

	// The slot a name refers to: a layer's name, or "Layer N" for an unnamed slot; -1 if neither
	static int LayerIndexFromName(std::string_view name)
	{
		int index = CollisionLayers::GetIndex(std::string(name));
		if (index >= 0)
			return index;

		constexpr std::string_view prefix = "Layer ";
		if (name.substr(0, prefix.size()) != prefix)
			return -1;
		std::string_view digits = name.substr(prefix.size());
		int slot = -1;
		auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), slot);
		if (error != std::errc() || end != digits.data() + digits.size())
			return -1;
		if (slot < 0 || slot >= CollisionLayers::MaxLayers || CollisionLayers::IsNamed(slot))
			return -1;
		return slot;
	}

	std::string LayerToText(uint16_t layer)
	{
		return CollisionLayers::GetLayerName(layer);
	}

	uint16_t LayerFromText(std::string_view text, bool* known, std::string_view where)
	{
		int index = LayerIndexFromName(Trim(text));
		if (known)
			*known = index >= 0;
		if (index < 0)
		{
			ENGINE_WARN("Unknown collision layer \"{0}\"{1}, using Default", std::string(text), std::string(where));
			return CollisionLayers::DefaultLayer;
		}
		return (uint16_t)(1u << index);
	}

	std::string MaskToText(uint16_t mask)
	{
		if (mask == CollisionLayers::Everything)
			return "Everything";
		if (mask == 0)
			return "Nothing";

		std::string text;
		for (int i = 0; i < CollisionLayers::MaxLayers; ++i)
		{
			if (!(mask & (1u << i)))
				continue;
			if (!text.empty())
				text += ", ";
			text += CollisionLayers::GetDisplayName(i);
		}
		return text;
	}

	uint16_t MaskFromText(std::string_view text, std::vector<std::string>* unknown, std::string_view where)
	{
		text = Trim(text);
		if (text == "Everything")
			return CollisionLayers::Everything;
		if (text == "Nothing" || text.empty())
			return 0;

		uint16_t mask = 0;
		while (!text.empty())
		{
			size_t comma = text.find(',');
			std::string_view name = Trim(text.substr(0, comma));
			text = comma == std::string_view::npos ? std::string_view() : text.substr(comma + 1);
			if (name.empty())
				continue;

			int index = LayerIndexFromName(name);
			if (index < 0)
			{
				ENGINE_WARN("Unknown collision layer \"{0}\" in a mask{1}, left out", std::string(name), std::string(where));
				if (unknown)
					unknown->push_back(std::string(name));
				continue;
			}
			mask |= (uint16_t)(1u << index);
		}
		return mask;
	}

	int32_t EnumFromText(const EnumType& type, std::string_view text, int32_t fallback, bool* known, std::string_view where)
	{
		std::optional<int32_t> value = type.ValueOf(Trim(text));
		if (known)
			*known = value.has_value();
		if (!value)
		{
			const char* fallbackName = type.NameOf(fallback);
			ENGINE_WARN("Unknown {0} \"{1}\"{2}, using {3}", type.name, std::string(text), std::string(where), fallbackName ? fallbackName : "the default");
			return fallback;
		}
		return *value;
	}

	int ComponentType::FindField(std::string_view fieldName) const
	{
		for (size_t i = 0; i < fields.size(); ++i)
		{
			if (fields[i].name == fieldName)
				return (int)i;
		}
		return -1;
	}

	void ChangeHook::operator()(Entity entity, void* component) const
	{
		if (function)
			call(function, entity, component);
	}

	namespace Detail
	{
		struct Registry
		{
			std::vector<Scope<ComponentType>> ownedComponents;
			std::vector<const ComponentType*> components;
			std::vector<Scope<EnumType>> ownedEnums;
			std::vector<const EnumType*> enums;
		};

		static Registry& GetRegistry()
		{
			static Registry registry;
			return registry;
		}

		ComponentType& AddComponent()
		{
			Registry& registry = GetRegistry();
			ComponentType& type = *registry.ownedComponents.emplace_back(CreateScope<ComponentType>());
			registry.components.push_back(&type);
			return type;
		}

		EnumType& AddEnum(std::string name, std::string description, std::vector<EnumType::Entry> entries)
		{
			CORE_ASSERT(!FindEnum(name), "Two enums with the same name");
			Registry& registry = GetRegistry();
			EnumType& type = *registry.ownedEnums.emplace_back(CreateScope<EnumType>());
			type.name = std::move(name);
			type.description = std::move(description);
			type.entries = std::move(entries);
			registry.enums.push_back(&type);
			return type;
		}
	}

	std::string SplitWords(const std::string& name)
	{
		std::string words;
		for (size_t i = 0; i < name.size(); ++i)
		{
			const char c = name[i];
			const bool upper = std::isupper((unsigned char)c);
			const bool previousLower = i > 0 && std::islower((unsigned char)name[i - 1]);
			const bool nextLower = i + 1 < name.size() && std::islower((unsigned char)name[i + 1]);
			const bool previousUpper = i > 0 && std::isupper((unsigned char)name[i - 1]);
			// A new word at "aB", or at the last capital of a run like "IDs" or "UIScale"
			if (i > 0 && upper && (previousLower || (previousUpper && nextLower)))
				words += ' ';
			words += c;
		}
		return words;
	}

	const std::vector<const ComponentType*>& Components()
	{
		return Detail::GetRegistry().components;
	}

	const ComponentType* FindComponent(std::string_view name)
	{
		for (const ComponentType* type : Components())
		{
			if (type->name == name)
				return type;
		}
		return nullptr;
	}

	const ComponentType* ExcludedBy(entt::registry& registry, entt::entity entity, const ComponentType& type)
	{
		for (const ComponentType* other : Components())
		{
			if (other == &type || !other->tryGet(registry, entity))
				continue;
			if (std::find(type.excludes.begin(), type.excludes.end(), other->name) != type.excludes.end()
				|| std::find(other->excludes.begin(), other->excludes.end(), type.name) != other->excludes.end())
				return other;
		}
		return nullptr;
	}

	const std::vector<const EnumType*>& Enums()
	{
		return Detail::GetRegistry().enums;
	}

	const EnumType* FindEnum(std::string_view name)
	{
		for (const EnumType* type : Enums())
		{
			if (type->name == name)
				return type;
		}
		return nullptr;
	}

	std::optional<PropertyPath> PropertyPath::Parse(std::string_view text)
	{
		auto fail = [text](const char* reason) -> std::optional<PropertyPath>
			{
				ENGINE_WARN("Property path \"{0}\": {1}", std::string(text), reason);
				return std::nullopt;
			};

		size_t dot = text.find('.');
		if (dot == std::string_view::npos)
			return fail("no field");

		PropertyPath path;
		path.component = FindComponent(text.substr(0, dot));
		if (!path.component)
			return fail("unknown component");

		std::string_view rest = text.substr(dot + 1);
		size_t end = rest.find_first_of(".[");
		path.field = path.component->FindField(rest.substr(0, end));
		if (path.field < 0)
			return fail("unknown field");
		rest = end == std::string_view::npos ? std::string_view() : rest.substr(end);

		const FieldInfo& field = path.Field();
		ValueType type = field.type;

		if (!rest.empty() && rest.front() == '[')
		{
			if (field.type != ValueType::List)
				return fail("an index on a field that isn't a list");
			size_t close = rest.find(']');
			if (close == std::string_view::npos)
				return fail("no closing ]");
			std::string_view digits = rest.substr(1, close - 1);
			auto [last, error] = std::from_chars(digits.data(), digits.data() + digits.size(), path.index);
			if (digits.empty() || error != std::errc() || last != digits.data() + digits.size() || path.index < 0)
				return fail("the index isn't a number");
			rest = rest.substr(close + 1);
			type = field.itemType;
		}

		if (!rest.empty())
		{
			if (rest.front() != '.')
				return fail("unexpected text after the field");
			std::string_view partName = rest.substr(1);
			for (int part = 0; part < PartCount(type); ++part)
			{
				if (partName == PartName(type, part))
					path.part = part;
			}
			if (path.part < 0)
				return fail("unknown part");
		}

		return path;
	}

	std::string PropertyPath::ToString() const
	{
		if (!component || field < 0)
			return std::string();

		const FieldInfo& info = Field();
		std::string text = component->name + "." + info.name;
		ValueType type = info.type;
		if (index >= 0)
		{
			text += "[" + std::to_string(index) + "]";
			type = info.itemType;
		}
		if (part >= 0)
			text += std::string(".") + PartName(type, part);
		return text;
	}

	ValueType PropertyPath::Type() const
	{
		if (part >= 0)
			return ValueType::Float;
		if (index >= 0)
			return Field().itemType;
		return Field().type;
	}

	static void* TryGetComponent(Entity entity, const ComponentType& type)
	{
		if (!entity || !entity.GetScene())
			return nullptr;
		return type.tryGet(entity.GetScene()->GetRegistry(), entity.GetHandle());
	}

	// The part of a field's value a path names
	static std::optional<Value> Extract(const Value& value, const PropertyPath& path)
	{
		const Value* item = &value;
		if (path.index >= 0)
		{
			const ValueList* items = value.TryAs<ValueList>();
			if (!items || path.index >= (int)items->size())
				return std::nullopt;
			item = &(*items)[path.index];
		}
		if (path.part >= 0)
		{
			std::optional<float> part = item->GetPart(path.part);
			if (!part)
				return std::nullopt;
			return Value(*part);
		}
		return *item;
	}

	// The field's value with the part a path names replaced
	static bool Replace(Value& fieldValue, const PropertyPath& path, const Value& value)
	{
		Value* item = &fieldValue;
		if (path.index >= 0)
		{
			if (!fieldValue.Is<ValueList>() || path.index >= (int)fieldValue.As<ValueList>().size())
				return false;
			item = &fieldValue.As<ValueList>()[path.index];
		}
		if (path.part >= 0)
		{
			const float* part = value.TryAs<float>();
			return part && item->SetPart(path.part, *part);
		}
		*item = value;
		return true;
	}

	std::optional<Value> Get(Entity entity, const PropertyPath& path)
	{
		if (!path.component || path.field < 0)
			return std::nullopt;
		const void* component = TryGetComponent(entity, *path.component);
		const FieldInfo& field = path.Field();
		if (!component || !field.get)
			return std::nullopt;
		return Extract(field.get(component), path);
	}

	static bool IsRunning(Entity entity)
	{
		return entity.GetScene()->IsRunning();
	}

	bool Set(Entity entity, const PropertyPath& path, const Value& value, ChangeSource source, uint32_t interaction)
	{
		if (!path.component || path.field < 0)
			return false;

		const std::string pathText = path.ToString();
		void* component = TryGetComponent(entity, *path.component);
		if (!component)
		{
			ENGINE_WARN("Can't set {0}: the entity doesn't have the component", pathText);
			return false;
		}

		const FieldInfo& field = path.Field();
		if (field.Has(FieldFlags::ReadOnly) || !field.set)
		{
			ENGINE_WARN("Can't set {0}: it's read-only", pathText);
			return false;
		}
		if (value.Type() != path.Type())
		{
			ENGINE_WARN("Can't set {0}, a {1}, to a {2}", pathText, ValueTypeName(path.Type()), ValueTypeName(value.Type()));
			return false;
		}

		Value oldFieldValue = field.get(component);
		Value newFieldValue = oldFieldValue;
		if (!Replace(newFieldValue, path, value))
		{
			ENGINE_WARN("Can't set {0}: there's no item {1}", pathText, path.index);
			return false;
		}
		if (newFieldValue == oldFieldValue)
			return true;

		// Undo sets the plain value, as loading does
		const bool useEdit = field.edit && source != ChangeSource::Undo;
		if (!(useEdit ? field.edit : field.set)(component, newFieldValue))
		{
			ENGINE_WARN("Can't set {0} to {1}: the value doesn't fit", pathText, value.ToString());
			return false;
		}

		field.onChanged(entity, component);
		path.component->onChanged(entity, component);

		Scene* scene = entity.GetScene();
		if (!IsRunning(entity))
			scene->MakeDirty();

		if (scene->HasPropertyChangedListeners())
		{
			std::optional<Value> oldValue = Extract(oldFieldValue, path);
			scene->SendPropertyChanged({ entity.GetID(), path, oldValue ? *oldValue : Value(), value, source, interaction });
		}
		return true;
	}

	static ComponentValues CaptureFrom(const ComponentType& type, const void* component)
	{
		ComponentValues captured;
		captured.type = &type;

		tinyxml2::XMLDocument document;
		tinyxml2::XMLElement* element = nullptr;
		for (const FieldInfo& field : type.fields)
		{
			if (!field.IsSaved())
				continue;
			if (field.IsCustom())
			{
				if (!element)
					element = document.InsertEndChild(document.NewElement(type.name.c_str()))->ToElement();
				field.save(component, element);
			}
			else
			{
				captured.values.push_back(field.get(component));
			}
		}

		if (element)
		{
			tinyxml2::XMLPrinter printer(nullptr, true);
			document.Print(&printer);
			captured.custom = printer.CStr();
		}
		return captured;
	}

	ComponentValues Capture(Entity entity, const ComponentType& type)
	{
		const void* component = TryGetComponent(entity, type);
		if (!component)
			return { &type, {}, {} };
		return CaptureFrom(type, component);
	}

	void Restore(Entity entity, const ComponentValues& values, ChangeSource source, uint32_t interaction)
	{
		CORE_ASSERT(values.type, "Restoring values with no component type");
		const ComponentType& type = *values.type;
		Scene* scene = entity.GetScene();

		bool changed = false;
		void* component = TryGetComponent(entity, type);
		if (!component)
		{
			component = type.emplace(scene->GetRegistry(), entity.GetHandle());
			changed = true;
		}

		const bool listening = scene->HasPropertyChangedListeners();
		struct Change
		{
			int field;
			Value oldValue;
		};
		std::vector<Change> changes;
		std::vector<ChangeHook> hooks;
		auto addHook = [&hooks](const ChangeHook& hook)
			{
				for (const ChangeHook& existing : hooks)
				{
					if (existing.function == hook.function)
						return;
				}
				if (hook)
					hooks.push_back(hook);
			};

		size_t next = 0;
		for (size_t i = 0; i < type.fields.size(); ++i)
		{
			const FieldInfo& field = type.fields[i];
			if (!field.IsSaved() || field.IsCustom())
				continue;
			CORE_ASSERT(next < values.values.size(), "Restoring fewer values than the component has");
			const Value& value = values.values[next++];
			if (field.Has(FieldFlags::ReadOnly))
				continue;

			Value oldValue = field.get(component);
			if (oldValue == value)
				continue;
			if (!field.set(component, value))
			{
				ENGINE_WARN("Can't restore {0}.{1} to {2}: the value doesn't fit", type.name, field.name, value.ToString());
				continue;
			}
			changed = true;
			addHook(field.onChanged);
			if (listening)
				changes.push_back({ (int)i, std::move(oldValue) });
		}

		if (!values.custom.empty())
		{
			tinyxml2::XMLDocument document;
			if (document.Parse(values.custom.c_str()) == tinyxml2::XML_SUCCESS && document.RootElement())
			{
				for (const FieldInfo& field : type.fields)
				{
					if (!field.IsCustom() || !field.IsSaved())
						continue;
					field.load(component, document.RootElement());
					addHook(field.onChanged);
					changed = true;
				}
			}
			else
			{
				ENGINE_WARN("Can't restore the custom fields of {0}: {1}", type.name, document.ErrorStr());
			}
		}

		if (!changed)
			return;

		for (const ChangeHook& hook : hooks)
			hook(entity, component);
		type.onChanged(entity, component);

		if (!scene->IsRunning())
			scene->MakeDirty();

		for (Change& change : changes)
		{
			PropertyPath path{ &type, change.field };
			scene->SendPropertyChanged({ entity.GetID(), path, std::move(change.oldValue), type.fields[change.field].get(component), source,
				interaction });
		}
	}

	ComponentValues Defaults(const ComponentType& type)
	{
		std::shared_ptr<void> component = type.create();
		return CaptureFrom(type, component.get());
	}
}
