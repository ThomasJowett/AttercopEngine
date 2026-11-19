#pragma once

#include "Core/UUID.h"

#include <algorithm>
#include <string>
#include <vector>

class Scene;

namespace Reflection
{
	struct ComponentType;
}

// What an open step edits: a step with another key closes the open one
struct EditKey
{
	// Sorted
	std::vector<Uuid> entities;
	// nullptr for structural edits
	const Reflection::ComponentType* component = nullptr;
	// "" for the whole component
	std::string propertyPath;

	EditKey() = default;
	EditKey(std::vector<Uuid> keyEntities, const Reflection::ComponentType* keyComponent, std::string keyPropertyPath = "")
		: entities(std::move(keyEntities)), component(keyComponent), propertyPath(std::move(keyPropertyPath))
	{
		std::sort(entities.begin(), entities.end());
	}

	bool operator==(const EditKey& other) const
	{
		return component == other.component && propertyPath == other.propertyPath && entities == other.entities;
	}
	bool operator!=(const EditKey& other) const { return !(*this == other); }
};

// One undo step on the open scene; holds entities by UUID and looks them up when it runs
class SceneCommand
{
public:
	virtual ~SceneCommand() = default;
	virtual void Undo(Scene& scene) = 0;
	virtual void Redo(Scene& scene) = 0;
	// Captures the "after" state; false if nothing changed
	virtual bool End(Scene& scene) { (void)scene; return true; }
	virtual std::string GetName() const = 0;
	// Bytes held, for the memory budget
	virtual size_t GetSize() const = 0;
};
