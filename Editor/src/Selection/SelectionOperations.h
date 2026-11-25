#pragma once

#include "Core/UUID.h"
#include "History/SceneHistory.h"
#include "Scene/Entity.h"
#include "Selection/EntitySelection.h"

#include <string>
#include <vector>

class Scene;

// What the Hierarchy's delete, duplicate, copy, paste and drop do to the selection, each one history step
namespace SelectionOperations
{
	enum class DropPosition { Before, Onto, After };

	// The top-level selected entities in hierarchy order
	std::vector<Entity> GetRoots(Scene& scene, const EntitySelection& selection);

	// Deletes the selection's top-level entities, with their children, and clears the selection
	void Delete(SceneHistory& history, EntitySelection& selection, Scene& scene);
	// Copies each top-level entity right after its original; the copies become the selection
	std::vector<Entity> Duplicate(SceneHistory& history, EntitySelection& selection, Scene& scene);
	// The clipboard text for the top-level selection, empty if nothing is selected
	std::string Copy(const EntitySelection& selection, Scene& scene);
	// Creates the entities in the text at the root with new UUIDs and selects them; none if the text has none
	std::vector<Entity> Paste(SceneHistory& history, EntitySelection& selection, Scene& scene, const std::string& text);
	// Hides every selected entity, or shows them all if they're all hidden, as one step
	void ToggleHidden(SceneHistory& history, const EntitySelection& selection, Scene& scene);
	// Moves entities onto, before or after the target (none for the root), keeping their order; false if refused
	bool Move(SceneHistory& history, Scene& scene, const std::vector<Uuid>& dragged, Entity target, DropPosition position);
}
