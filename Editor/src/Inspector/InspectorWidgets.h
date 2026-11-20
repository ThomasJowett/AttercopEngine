#pragma once

#include "Reflection/ComponentType.h"
#include "Viewers/ViewerManager.h"

#include <string_view>

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
}
