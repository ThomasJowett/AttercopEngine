#include "InspectorWidgets.h"

#include "Reflection/Reflection.h"
#include "Scene/Entity.h"
#include "Scene/SceneGraph.h"

#include <cctype>

namespace Inspector
{
	static const AssetKind s_AssetKinds[] = {
		{ "Texture2D", FileType::IMAGE },
		{ "Material", FileType::MATERIAL },
		{ "PhysicsMaterial", FileType::PHYSICSMATERIAL },
		{ "Font", FileType::FONT },
		{ "AudioClip", FileType::AUDIO },
		{ "Tileset", FileType::TILESET },
		{ "SpriteSheet", FileType::SPRITESHEET },
		{ "StaticMesh", FileType::MESH },
		{ "LuaScript", FileType::SCRIPT },
		{ "BehaviourTree", FileType::BEHAVIOURTREE },
		{ "StateMachine", FileType::STATEMACHINE },
	};

	const AssetKind* FindAssetKind(std::string_view type)
	{
		for (const AssetKind& kind : s_AssetKinds)
		{
			if (type == kind.type)
				return &kind;
		}
		return nullptr;
	}

	static Widget WidgetFor(Reflection::ValueType type, const Reflection::FieldInfo& field)
	{
		using Reflection::ValueType;
		switch (type)
		{
		case ValueType::Bool: return Widget::Checkbox;
		case ValueType::Int: return Widget::DragInt;
		case ValueType::UInt: return Widget::DragUInt;
		case ValueType::Float: return Widget::DragFloat;
		case ValueType::String:
			if (field.choices)
				return Widget::Choice;
			return field.Has(Reflection::FieldFlags::AsText) ? Widget::MultilineText : Widget::Text;
		case ValueType::Vec2:
		case ValueType::Vec3: return Widget::Vector;
		case ValueType::Quaternion: return Widget::None;
		case ValueType::Colour: return Widget::Colour;
		case ValueType::Enum: return field.enumType ? Widget::Combo : Widget::None;
		case ValueType::Asset: return FindAssetKind(field.assetType) ? Widget::Asset : Widget::None;
		case ValueType::Layer: return Widget::Layer;
		case ValueType::Mask: return Widget::Mask;
		case ValueType::Entity: return Widget::Entity;
		case ValueType::List: return Widget::List;
		}
		return Widget::None;
	}

	Widget WidgetFor(const Reflection::FieldInfo& field)
	{
		if (field.IsCustom())
			return Widget::None;
		if (field.type == Reflection::ValueType::List)
		{
			Widget item = WidgetFor(field.itemType, field);
			return item == Widget::None || item == Widget::List ? Widget::None : Widget::List;
		}
		return WidgetFor(field.type, field);
	}

	std::string HierarchyPath(Entity entity)
	{
		std::string path = entity.GetName();
		for (Entity parent = entity.GetParent(); parent; parent = parent.GetParent())
			path = parent.GetName() + "/" + path;
		return path;
	}

	static bool ContainsIgnoringCase(std::string_view text, std::string_view search)
	{
		if (search.size() > text.size())
			return false;
		for (size_t start = 0; start + search.size() <= text.size(); ++start)
		{
			size_t i = 0;
			while (i < search.size() && std::tolower((unsigned char)text[start + i]) == std::tolower((unsigned char)search[i]))
				++i;
			if (i == search.size())
				return true;
		}
		return false;
	}

	static void AddChoices(Entity entity, const std::string& parentPath, const Reflection::FieldInfo& field, std::string_view search,
		std::vector<EntityChoice>& choices)
	{
		const std::string path = parentPath.empty() ? entity.GetName() : parentPath + "/" + entity.GetName();
		if (Reflection::MeetsRequirements(field, entity) && ContainsIgnoringCase(path, search))
			choices.push_back({ entity.GetID(), path });
		for (Entity child : SceneGraph::GetChildren(entity))
			AddChoices(child, path, field, search, choices);
	}

	std::vector<EntityChoice> EntityChoices(Scene& scene, const Reflection::FieldInfo& field, std::string_view search)
	{
		std::vector<EntityChoice> choices;
		for (Entity root : SceneGraph::GetRoots(scene))
			AddChoices(root, std::string(), field, search, choices);
		return choices;
	}
}
