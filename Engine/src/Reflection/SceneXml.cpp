#include "Reflection/SceneXml.h"

#include "Reflection/Reflection.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Asset/Asset.h"
#include "Asset/Material.h"
#include "Logging/Logger.h"
#include "Utilities/SerializationUtils.h"

#include "TinyXml2/tinyxml2.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace Reflection
{
	namespace SceneXml
	{
		namespace
		{
			bool IsAttribute(ValueType type)
			{
				switch (type)
				{
				case ValueType::Bool:
				case ValueType::Int:
				case ValueType::UInt:
				case ValueType::Float:
				case ValueType::String:
				case ValueType::Enum:
				case ValueType::Layer:
				case ValueType::Mask:
					return true;
				default:
					return false;
				}
			}

			bool IsWritten(const FieldInfo& field)
			{
				return field.IsSaved() && !field.Has(FieldFlags::ReadOnly);
			}

			// Where a field goes in its component's (or group's) element
			enum class Placement { Attribute, Text, MainAsset, Element, Items, Custom };

			Placement PlacementOf(const FieldInfo& field)
			{
				if (field.IsCustom())
					return Placement::Custom;
				if (field.type == ValueType::List)
					return Placement::Items;
				if (field.type == ValueType::Asset)
					return field.Has(FieldFlags::MainAsset) ? Placement::MainAsset : Placement::Element;
				if (field.type == ValueType::String && field.Has(FieldFlags::AsText))
					return Placement::Text;
				return IsAttribute(field.type) ? Placement::Attribute : Placement::Element;
			}

			// What a component's element may hold, and the default of each asset field, built once per type
			struct Layout
			{
				struct Names
				{
					std::vector<std::string> attributes;
					std::vector<std::string> elements;

					static bool Contains(const std::vector<std::string>& names, const char* name)
					{
						for (const std::string& known : names)
						{
							if (known == name)
								return true;
						}
						return false;
					}
				};

				Names component;
				std::vector<Names> groups;
				// A default component's asset path per field, empty for other fields
				std::vector<std::filesystem::path> defaultAssets;
			};

			void AddName(std::vector<std::string>& names, const std::string& name)
			{
				if (!name.empty() && !Layout::Names::Contains(names, name.c_str()))
					names.push_back(name);
			}

			const Layout& LayoutOf(const ComponentType& type)
			{
				static std::unordered_map<const ComponentType*, Layout> s_Layouts;
				auto found = s_Layouts.find(&type);
				if (found != s_Layouts.end())
					return found->second;

				Layout layout;
				layout.groups.resize(type.groups.size());
				layout.defaultAssets.resize(type.fields.size());
				std::shared_ptr<void> defaults = type.create();

				for (size_t i = 0; i < type.fields.size(); ++i)
				{
					const FieldInfo& field = type.fields[i];
					if (!IsWritten(field))
						continue;
					Layout::Names& names = field.group >= 0 ? layout.groups[field.group] : layout.component;
					switch (PlacementOf(field))
					{
					case Placement::Attribute: AddName(names.attributes, field.xmlName); break;
					case Placement::MainAsset: AddName(names.attributes, "Filepath"); break;
					case Placement::Element: AddName(names.elements, field.xmlName); break;
					case Placement::Items: AddName(names.elements, field.itemName); break;
					case Placement::Custom: AddName(names.elements, field.itemName); break;
					case Placement::Text: break;
					}
					if (field.group >= 0)
						AddName(layout.component.elements, type.groups[field.group].name);
					if (field.type == ValueType::Asset && !field.IsCustom())
					{
						Value value = field.get(defaults.get());
						layout.defaultAssets[i] = value.As<AssetValue>().path;
					}
				}
				return s_Layouts.emplace(&type, std::move(layout)).first->second;
			}

			void* FindComponentOf(Entity entity, const ComponentType& type)
			{
				return type.tryGet(entity.GetScene()->GetRegistry(), entity.GetHandle());
			}

			/* ------------------------------------------------------------------------------------------------------ */

			void WriteParts(tinyxml2::XMLElement* element, const Value& value)
			{
				for (int part = 0; part < PartCount(value.Type()); ++part)
					element->SetAttribute(PartName(value.Type(), part), FloatToText(*value.GetPart(part)).c_str());
			}

			std::string ScalarToText(const Value& value)
			{
				switch (value.Type())
				{
				case ValueType::Bool: return value.As<bool>() ? "true" : "false";
				case ValueType::Int: return std::to_string(value.As<int32_t>());
				case ValueType::UInt: return std::to_string(value.As<uint32_t>());
				case ValueType::Float: return FloatToText(value.As<float>());
				case ValueType::String: return value.As<std::string>();
				case ValueType::Enum:
				{
					const EnumValue& enumValue = value.As<EnumValue>();
					const char* name = enumValue.type ? enumValue.type->NameOf(enumValue.value) : nullptr;
					return name ? name : std::to_string(enumValue.value);
				}
				case ValueType::Layer: return LayerToText(value.As<LayerValue>().bits);
				case ValueType::Mask: return MaskToText(value.As<MaskValue>().bits);
				default: return {};
				}
			}

			// A list item, or a vector, colour or asset field, as an element of its own
			void WriteValueElement(tinyxml2::XMLElement* element, const Value& value)
			{
				if (PartCount(value.Type()) > 0)
					WriteParts(element, value);
				else if (const AssetValue* asset = value.TryAs<AssetValue>())
					SerializationUtils::Encode(element, asset->path);
				else
					element->SetAttribute("Value", ScalarToText(value).c_str());
			}

			void WriteField(tinyxml2::XMLElement* element, const FieldInfo& field, const void* component,
				const std::filesystem::path& defaultAsset)
			{
				switch (PlacementOf(field))
				{
				case Placement::Custom:
					field.save(component, element);
					return;
				case Placement::Attribute:
					element->SetAttribute(field.xmlName.c_str(), ScalarToText(field.get(component)).c_str());
					return;
				case Placement::Text:
					element->SetText(field.get(component).As<std::string>().c_str());
					return;
				case Placement::MainAsset:
				case Placement::Element:
				{
					Value value = field.get(component);
					if (const AssetValue* asset = value.TryAs<AssetValue>())
					{
						// Unset, the component's default, as the default font, or the default material
						if (!asset->IsSet() || (!defaultAsset.empty() && asset->path == defaultAsset)
							|| asset->path == Material::DefaultFilepath)
							return;
						if (field.Has(FieldFlags::MainAsset))
						{
							SerializationUtils::Encode(element, asset->path);
							return;
						}
					}
					WriteValueElement(element->InsertNewChildElement(field.xmlName.c_str()), value);
					return;
				}
				case Placement::Items:
				{
					Value value = field.get(component);
					for (const Value& item : value.As<ValueList>())
						WriteValueElement(element->InsertNewChildElement(field.itemName.c_str()), item);
					return;
				}
				}
			}

			/* ------------------------------------------------------------------------------------------------------ */

			struct Reader
			{
				const std::filesystem::path& file;

				void Warn(const tinyxml2::XMLElement* element, const std::string& message) const
				{
					ENGINE_WARN("{0}{1}", message, Where(file, element->GetLineNum()));
				}

				void CheckNames(const tinyxml2::XMLElement* element, const Layout::Names& names) const
				{
					for (const tinyxml2::XMLAttribute* attribute = element->FirstAttribute(); attribute; attribute = attribute->Next())
					{
						if (!Layout::Names::Contains(names.attributes, attribute->Name()))
							Warn(element, std::string("Unknown attribute ") + attribute->Name() + " on <" + element->Name() + ">, ignored");
					}
					for (const tinyxml2::XMLElement* child = element->FirstChildElement(); child; child = child->NextSiblingElement())
					{
						if (!Layout::Names::Contains(names.elements, child->Name()))
							Warn(child, std::string("Unknown element <") + child->Name() + "> in <" + element->Name() + ">, ignored");
					}
				}

				// The parts present in the element replace those of current
				bool ReadParts(const tinyxml2::XMLElement* element, Value& value) const
				{
					const int count = PartCount(value.Type());
					for (int part = 0; part < count; ++part)
					{
						const char* name = PartName(value.Type(), part);
						const tinyxml2::XMLAttribute* attribute = element->FindAttribute(name);
						if (!attribute)
							continue;
						float number = 0.0f;
						if (attribute->QueryFloatValue(&number) != tinyxml2::XML_SUCCESS)
						{
							Warn(element, std::string("Can't read ") + name + "=\"" + attribute->Value() + "\" as a number");
							continue;
						}
						value.SetPart(part, number);
					}
					for (const tinyxml2::XMLAttribute* attribute = element->FirstAttribute(); attribute; attribute = attribute->Next())
					{
						bool known = false;
						for (int part = 0; part < count; ++part)
							known = known || std::strcmp(attribute->Name(), PartName(value.Type(), part)) == 0;
						if (!known)
							Warn(element, std::string("Unknown attribute ") + attribute->Name() + " on <" + element->Name() + ">, ignored");
					}
					return true;
				}

				// nullopt, with a warning, if the text doesn't fit the field
				// component is read for the fallback of an unknown enum name, and may be null for a list item
				std::optional<Value> ReadScalar(const tinyxml2::XMLElement* element, const char* name, const char* text,
					const FieldInfo& field, ValueType type, const void* component) const
				{
					auto cantRead = [&]() -> std::optional<Value>
						{
							ENGINE_WARN("Can't read {0}=\"{1}\" as a {2}{3}", name, text, ValueTypeName(type), Where(file, element->GetLineNum()));
							return std::nullopt;
						};
					switch (type)
					{
					case ValueType::Bool:
					{
						bool value = false;
						if (tinyxml2::XMLUtil::ToBool(text, &value))
							return Value(value);
						return cantRead();
					}
					case ValueType::Int:
					{
						int value = 0;
						if (tinyxml2::XMLUtil::ToInt(text, &value))
							return Value((int32_t)value);
						return cantRead();
					}
					case ValueType::UInt:
					{
						unsigned value = 0;
						if (tinyxml2::XMLUtil::ToUnsigned(text, &value))
							return Value((uint32_t)value);
						return cantRead();
					}
					case ValueType::Float:
					{
						float value = 0.0f;
						if (tinyxml2::XMLUtil::ToFloat(text, &value))
							return Value(value);
						return cantRead();
					}
					case ValueType::String:
						return Value(std::string(text));
					case ValueType::Enum:
					{
						if (std::optional<int32_t> known = field.enumType->ValueOf(text))
							return Value(EnumValue{ field.enumType, *known });
						const EnumValue* current = component && type == field.type ? field.get(component).TryAs<EnumValue>() : nullptr;
						const int32_t fallback = current ? current->value : 0;
						return Value(EnumValue{ field.enumType,
							EnumFromText(*field.enumType, text, fallback, nullptr, Where(file, element->GetLineNum())) });
					}
					case ValueType::Layer:
						return Value(LayerValue{ LayerFromText(text, nullptr, Where(file, element->GetLineNum())) });
					case ValueType::Mask:
						return Value(MaskValue{ MaskFromText(text, nullptr, Where(file, element->GetLineNum())) });
					default:
						return std::nullopt;
					}
				}

				// A list item, or a vector, colour or asset field, from its own element
				std::optional<Value> ReadValueElement(const tinyxml2::XMLElement* element, const FieldInfo& field, ValueType type,
					Value current) const
				{
					if (PartCount(type) > 0)
					{
						ReadParts(element, current);
						return current;
					}
					if (type == ValueType::Asset)
					{
						std::filesystem::path path;
						SerializationUtils::Decode(element, path);
						return Value(AssetValue{ path, nullptr });
					}
					const char* text = element->Attribute("Value");
					if (!text)
						return std::nullopt;
					return ReadScalar(element, "Value", text, field, type, nullptr);
				}

				// Default of a list item, for items read from scratch
				static Value ItemDefault(ValueType type)
				{
					switch (type)
					{
					case ValueType::Vec2: return Vector2f();
					case ValueType::Vec3: return Vector3f();
					case ValueType::Colour: return Colour();
					case ValueType::Asset: return AssetValue{};
					default: return Value();
					}
				}

				// False if the field wasn't in the element
				bool ReadField(const tinyxml2::XMLElement* element, const FieldInfo& field, void* component) const
				{
					std::optional<Value> value;
					switch (PlacementOf(field))
					{
					case Placement::Custom:
						field.load(component, element);
						return true;
					case Placement::Attribute:
					{
						const char* text = element->Attribute(field.xmlName.c_str());
						if (!text)
							return false;
						value = ReadScalar(element, field.xmlName.c_str(), text, field, field.type, component);
						break;
					}
					case Placement::Text:
					{
						const char* text = element->GetText();
						if (!text)
							return false;
						value = Value(std::string(text));
						break;
					}
					case Placement::MainAsset:
					{
						if (!element->Attribute("Filepath"))
							return false;
						std::filesystem::path path;
						SerializationUtils::Decode(element, path);
						value = Value(AssetValue{ path, nullptr });
						break;
					}
					case Placement::Element:
					{
						const tinyxml2::XMLElement* child = element->FirstChildElement(field.xmlName.c_str());
						if (!child)
							return false;
						value = ReadValueElement(child, field, field.type, field.get(component));
						break;
					}
					case Placement::Items:
					{
						const tinyxml2::XMLElement* item = element->FirstChildElement(field.itemName.c_str());
						// No items keeps the default, as a polygon's default shape
						if (!item)
							return false;
						ValueList items;
						for (; item; item = item->NextSiblingElement(field.itemName.c_str()))
						{
							if (std::optional<Value> itemValue = ReadValueElement(item, field, field.itemType, ItemDefault(field.itemType)))
								items.push_back(std::move(*itemValue));
						}
						value = Value(std::move(items));
						break;
					}
					}

					if (!value)
						return false;
					if (!field.set(component, *value))
					{
						Warn(element, "Can't set " + field.name + " to " + value->ToString());
						return false;
					}
					return true;
				}
			};
		}

		/* ---------------------------------------------------------------------------------------------------------- */

		std::string FloatToText(float value)
		{
			char text[32];
			for (int precision = 6; precision <= 9; ++precision)
			{
				std::snprintf(text, sizeof(text), "%.*g", precision, (double)value);
				if (std::strtof(text, nullptr) == value)
					return text;
			}
			return text;
		}

		std::string Where(const std::filesystem::path& file, int line)
		{
			return " (" + file.string() + ", line " + std::to_string(line) + ")";
		}

		/* ---------------------------------------------------------------------------------------------------------- */

		void WriteComponent(tinyxml2::XMLElement* entityElement, Entity entity, const ComponentType& type)
		{
			const void* component = FindComponentOf(entity, type);
			if (!component)
				return;

			const Layout& layout = LayoutOf(type);
			tinyxml2::XMLElement* element = entityElement->InsertNewChildElement(type.name.c_str());
			std::vector<tinyxml2::XMLElement*> groupElements(type.groups.size(), nullptr);

			for (size_t i = 0; i < type.fields.size(); ++i)
			{
				const FieldInfo& field = type.fields[i];
				if (!IsWritten(field))
					continue;

				if (!field.saveIf(component))
					continue;
				tinyxml2::XMLElement* target = element;
				if (field.group >= 0)
				{
					const GroupInfo& group = type.groups[field.group];
					if (!group.when(component))
						continue;
					if (!groupElements[field.group])
						groupElements[field.group] = element->InsertNewChildElement(group.name.c_str());
					target = groupElements[field.group];
				}
				WriteField(target, field, component, layout.defaultAssets[i]);
			}
		}

		void WriteComponents(tinyxml2::XMLElement* entityElement, Entity entity)
		{
			for (const ComponentType* type : Components())
			{
				if (!type->IsInternal())
					WriteComponent(entityElement, entity, *type);
			}
		}

		/* ---------------------------------------------------------------------------------------------------------- */

		void ReadComponent(const tinyxml2::XMLElement* element, Entity entity, const ComponentType& type,
			const std::filesystem::path& file)
		{
			entt::registry& registry = entity.GetScene()->GetRegistry();
			void* component = type.tryGet(registry, entity.GetHandle());
			if (!component)
				component = type.emplace(registry, entity.GetHandle());

			const Layout& layout = LayoutOf(type);
			const Reader reader{ file };
			reader.CheckNames(element, layout.component);

			std::vector<const tinyxml2::XMLElement*> groupElements(type.groups.size(), nullptr);
			for (size_t group = 0; group < type.groups.size(); ++group)
			{
				groupElements[group] = element->FirstChildElement(type.groups[group].name.c_str());
				if (groupElements[group])
					reader.CheckNames(groupElements[group], layout.groups[group]);
			}

			// Each hook once, in the order the fields first ask for it
			std::array<ChangeHook, 16> hooks;
			size_t hookCount = 0;
			for (const FieldInfo& field : type.fields)
			{
				if (!IsWritten(field))
					continue;
				const tinyxml2::XMLElement* source = field.group >= 0 ? groupElements[field.group] : element;
				if (!source || !reader.ReadField(source, field, component) || !field.onChanged)
					continue;
				bool seen = false;
				for (size_t i = 0; i < hookCount; ++i)
					seen = seen || hooks[i].function == field.onChanged.function;
				if (!seen)
				{
					CORE_ASSERT(hookCount < hooks.size(), "More field hooks than SceneXml reads");
					hooks[hookCount++] = field.onChanged;
				}
			}

			for (size_t i = 0; i < hookCount; ++i)
				hooks[i](entity, component);
			type.onChanged(entity, component);
		}

		void ReadComponents(const tinyxml2::XMLElement* entityElement, Entity entity, const std::filesystem::path& file)
		{
			static std::unordered_map<std::string, size_t> s_Indices;
			const std::vector<const ComponentType*>& types = Components();
			if (s_Indices.size() != types.size())
			{
				s_Indices.clear();
				for (size_t i = 0; i < types.size(); ++i)
					s_Indices.emplace(types[i]->name, i);
			}

			// Found by name, then read in registration order
			struct Found
			{
				size_t index;
				const tinyxml2::XMLElement* element;
			};
			std::array<Found, 64> found;
			size_t count = 0;
			for (const tinyxml2::XMLElement* child = entityElement->FirstChildElement(); child; child = child->NextSiblingElement())
			{
				const char* name = child->Name();
				if (std::strcmp(name, "Entity") == 0)
					continue;
				auto index = s_Indices.find(name);
				if (index == s_Indices.end() || types[index->second]->IsInternal())
				{
					ENGINE_WARN("Unknown component <{0}>, ignored{1}", name, Where(file, child->GetLineNum()));
					continue;
				}
				size_t at = count;
				while (at > 0 && found[at - 1].index > index->second)
					--at;
				if ((at > 0 && found[at - 1].index == index->second) || count == found.size())
				{
					ENGINE_WARN("A second <{0}> on one entity, ignored{1}", name, Where(file, child->GetLineNum()));
					continue;
				}
				for (size_t i = count; i > at; --i)
					found[i] = found[i - 1];
				found[at] = { index->second, child };
				++count;
			}

			for (size_t i = 0; i < count; ++i)
				ReadComponent(found[i].element, entity, *types[found[i].index], file);
		}
	
		/* ---------------------------------------------------------------------------------------------------------- */

		namespace
		{
			// The slot a name refers to in a set of names, -1 if none
			int SlotOf(std::string_view name, const CollisionLayers::Names& names)
			{
				while (!name.empty() && std::isspace((unsigned char)name.front()))
					name.remove_prefix(1);
				while (!name.empty() && std::isspace((unsigned char)name.back()))
					name.remove_suffix(1);
				for (int i = 0; i < CollisionLayers::MaxLayers; ++i)
				{
					if (!names[i].empty() && names[i] == name)
						return i;
				}
				for (int i = 0; i < CollisionLayers::MaxLayers; ++i)
				{
					if (names[i].empty() && name == "Layer " + std::to_string(i))
						return i;
				}
				return -1;
			}

			std::string RenamedLayer(std::string_view name, const CollisionLayers::Names& oldNames, const CollisionLayers::Names& newNames)
			{
				const int slot = SlotOf(name, oldNames);
				if (slot < 0)
					return std::string(name);
				return newNames[slot].empty() ? "Layer " + std::to_string(slot) : newNames[slot];
			}

			std::string RenamedMask(std::string_view text, const CollisionLayers::Names& oldNames, const CollisionLayers::Names& newNames)
			{
				if (text == "Everything" || text == "Nothing")
					return std::string(text);
				std::string renamed;
				while (!text.empty())
				{
					const size_t comma = text.find(',');
					std::string_view name = text.substr(0, comma);
					text = comma == std::string_view::npos ? std::string_view() : text.substr(comma + 1);
					while (!name.empty() && std::isspace((unsigned char)name.front()))
						name.remove_prefix(1);
					if (!name.empty())
						renamed += (renamed.empty() ? "" : ", ") + RenamedLayer(name, oldNames, newNames);
				}
				return renamed;
			}

			bool RenameInEntity(tinyxml2::XMLElement* entity, const CollisionLayers::Names& oldNames, const CollisionLayers::Names& newNames)
			{
				bool changed = false;
				for (tinyxml2::XMLElement* child = entity->FirstChildElement(); child; child = child->NextSiblingElement())
				{
					if (std::strcmp(child->Name(), "Entity") == 0)
					{
						changed = RenameInEntity(child, oldNames, newNames) || changed;
						continue;
					}
					const ComponentType* type = FindComponent(child->Name());
					if (!type)
						continue;
					for (const FieldInfo& field : type->fields)
					{
						if ((field.type != ValueType::Layer && field.type != ValueType::Mask) || field.group >= 0)
							continue;
						const char* text = child->Attribute(field.xmlName.c_str());
						if (!text)
							continue;
						const std::string renamed = field.type == ValueType::Layer
							? RenamedLayer(text, oldNames, newNames) : RenamedMask(text, oldNames, newNames);
						if (renamed != text)
						{
							child->SetAttribute(field.xmlName.c_str(), renamed.c_str());
							changed = true;
						}
					}
				}
				return changed;
			}
		}

		bool RenameCollisionLayers(tinyxml2::XMLElement* root, const CollisionLayers::Names& oldNames,
			const CollisionLayers::Names& newNames)
		{
			if (std::strcmp(root->Name(), "Entity") == 0)
				return RenameInEntity(root, oldNames, newNames);
			bool changed = false;
			for (tinyxml2::XMLElement* entity = root->FirstChildElement("Entity"); entity; entity = entity->NextSiblingElement("Entity"))
				changed = RenameInEntity(entity, oldNames, newNames) || changed;
			return changed;
		}
	}
}
