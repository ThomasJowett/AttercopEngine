#pragma once

#include "Core/UUID.h"
#include "Reflection/ComponentType.h"
#include "Viewers/ViewerManager.h"

#include <string>
#include <string_view>
#include <vector>

class Entity;
class Scene;

// The inspector's widget for each value type, without ImGui
namespace Inspector
{
	enum class Widget
	{
		None,
		Checkbox,
		DragInt,
		DragUInt,
		DragFloat,
		Text,
		MultilineText,
		Choice,
		Vector,
		Colour,
		Combo,
		Asset,
		Layer,
		Mask,
		Entity,
		List
	};

	// None for a field only an override draws, as a custom one
	Widget WidgetFor(const Reflection::FieldInfo& field);

	struct AssetKind
	{
		const char* type;
		FileType fileType;
	};

	// By the field's asset type, "Texture2D" or "BehaviourTree"; nullptr if the inspector can't pick it
	const AssetKind* FindAssetKind(std::string_view type);

	// "Level/Doors/Door 2"
	std::string HierarchyPath(Entity entity);

	struct EntityChoice
	{
		Uuid id;
		std::string path;
	};
	// The entities an entity field can point at, by hierarchy path in hierarchy order: those with a component it
	// requires, whose path contains the search text, ignoring case
	std::vector<EntityChoice> EntityChoices(Scene& scene, const Reflection::FieldInfo& field, std::string_view search = {});
}
