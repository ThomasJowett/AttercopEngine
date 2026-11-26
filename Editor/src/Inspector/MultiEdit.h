#pragma once

#include "Reflection/Reflection.h"
#include "Scene/Entity.h"

#include <array>
#include <functional>
#include <optional>
#include <vector>

class SceneHistory;

// Reading and changing a property or component across every selected entity, without ImGui
namespace MultiEdit
{
	struct ComponentSplit
	{
		// On every entity, in registration order
		std::vector<const Reflection::ComponentType*> common;
		// On some of the entities but not all
		std::vector<const Reflection::ComponentType*> partial;
	};
	ComponentSplit SplitComponents(const std::vector<Entity>& entities);

	struct FieldState
	{
		// The first entity's value
		Reflection::Value value;
		bool mixed = false;
		// Per part of a vector or colour
		std::array<bool, 4> partMixed{};
	};
	// The field's value on the first entity and whether the others differ; nullopt if an entity lacks the component
	std::optional<FieldState> ReadField(const std::vector<Entity>& entities, const Reflection::ComponentType& type, int field);

	// Adds the component to each entity that lacks it and can take it, as one step; add puts it on one entity
	void AddComponent(SceneHistory* history, const std::vector<Entity>& entities, const Reflection::ComponentType& type,
		const std::function<void(Entity)>& add);
	// Removes the component from every entity that has it, as one step
	void RemoveComponent(SceneHistory* history, const std::vector<Entity>& entities, const Reflection::ComponentType& type);
	// Whether an entity lacks the component and nothing on it excludes it
	bool CanAdd(Entity entity, const Reflection::ComponentType& type);
}
