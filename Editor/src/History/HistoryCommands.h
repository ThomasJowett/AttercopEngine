#pragma once
#include "History/SceneCommand.h"
#include "Scene/Entity.h"
#include "Reflection/Reflection.h"
#include "Core/UUID.h"

#include <optional>
#include <string>
#include <vector>

namespace HistoryCommands
{
	// The entity with this UUID, or a null entity and a warning
	Entity Find(Scene& scene, const Uuid& id);
	// The entity with this UUID, or a null entity if there's no UUID
	Entity FindOptional(Scene& scene, const std::optional<Uuid>& id);
	// Bytes held by captured values
	size_t SizeOf(const Reflection::Value& value);
	size_t SizeOf(const Reflection::ComponentValues& values);
}

// The key of an edit of one entity's whole component
EditKey ComponentEditKey(Entity entity, const Reflection::ComponentType& type);

template<typename T>
EditKey ComponentEditKey(Entity entity)
{
	return ComponentEditKey(entity, Reflection::TypeOf<T>());
}

// Where an entity sits: its parent, and the sibling after it or none for the last; roots have no parent
struct EntityPosition
{
	Uuid entity;
	std::optional<Uuid> parent;
	std::optional<Uuid> nextSibling;

	bool operator==(const EntityPosition& other) const
	{
		return entity == other.entity && parent == other.parent && nextSibling == other.nextSibling;
	}
	bool operator!=(const EntityPosition& other) const { return !(*this == other); }
};

// An entity and its children in the per-entity serialised form copy and paste use, and where it sits
struct EntitySubtree
{
	EntityPosition position;
	std::string serialised;
	std::string name;
};

namespace HistoryCommands
{
	// The entities in hierarchy order, leaving out any under another one in the list
	std::vector<Entity> TopLevelInOrder(const std::vector<Entity>& entities);
	EntityPosition PositionOf(Entity entity);
	// Puts the entity under its parent, then before its next sibling
	void MoveTo(Scene& scene, Entity entity, const EntityPosition& position);
	EntitySubtree SaveSubtree(Entity entity);
	// Re-creates the subtrees with their UUIDs and puts them back in place; subtrees are in hierarchy order
	void RestoreSubtrees(Scene& scene, const std::vector<EntitySubtree>& subtrees);
	void RemoveSubtrees(Scene& scene, const std::vector<EntitySubtree>& subtrees);
}

class CreateEntitiesCommand : public SceneCommand
{
public:
	// Construct once the roots are complete, with their components, children and parents
	explicit CreateEntitiesCommand(const std::vector<Entity>& roots);
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	bool End(Scene& scene) override;
	std::string GetName() const override;
	size_t GetSize() const override;
	std::vector<Uuid> GetSelectionAfterRedo() const override;
private:
	std::vector<Uuid> m_Roots;
	std::vector<EntitySubtree> m_Subtrees;
};

class DeleteEntitiesCommand : public SceneCommand
{
public:
	// Construct before the roots are deleted
	explicit DeleteEntitiesCommand(const std::vector<Entity>& roots);
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	std::string GetName() const override;
	size_t GetSize() const override;
	std::vector<Uuid> GetSelectionAfterUndo() const override;
private:
	std::vector<EntitySubtree> m_Subtrees;
};

// Moves entities to another parent or place among their siblings
class ReparentEntitiesCommand : public SceneCommand
{
public:
	// Construct before the move
	explicit ReparentEntitiesCommand(const std::vector<Entity>& entities);
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	bool End(Scene& scene) override;
	std::string GetName() const override;
	size_t GetSize() const override;
	std::vector<Uuid> GetSelectionAfterUndo() const override { return m_Entities; }
	std::vector<Uuid> GetSelectionAfterRedo() const override { return m_Entities; }
private:
	std::vector<Uuid> m_Entities;
	std::string m_Name;
	// Each in hierarchy order at its time
	std::vector<EntityPosition> m_Before;
	std::vector<EntityPosition> m_After;
};

// Several commands as one step, undone in reverse
class CompoundCommand : public SceneCommand
{
public:
	explicit CompoundCommand(std::string name);
	// Adds a command that has already ended
	void Append(Scope<SceneCommand> command);
	bool IsEmpty() const { return m_Commands.empty(); }
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	std::string GetName() const override;
	size_t GetSize() const override;
	std::vector<Uuid> GetSelectionAfterUndo() const override;
	std::vector<Uuid> GetSelectionAfterRedo() const override;
private:
	std::string m_Name;
	std::vector<Scope<SceneCommand>> m_Commands;
};

// Adds the component and records it, as the Add Component menu does
class AddComponentCommand : public SceneCommand
{
public:
	// Construct before adding the component
	AddComponentCommand(Entity entity, const Reflection::ComponentType& type);
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	bool End(Scene& scene) override;
	std::string GetName() const override;
	size_t GetSize() const override;
private:
	Uuid m_Entity;
	Reflection::ComponentValues m_Added;
};

class RemoveComponentCommand : public SceneCommand
{
public:
	// Construct before removing the component
	RemoveComponentCommand(Entity entity, const Reflection::ComponentType& type);
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	std::string GetName() const override;
	size_t GetSize() const override;
private:
	Uuid m_Entity;
	Reflection::ComponentValues m_Removed;
};

// A whole component's saved values before and after an edit, put back through its description
class EditComponentCommand : public SceneCommand
{
public:
	// Construct before the edit
	EditComponentCommand(Entity entity, const Reflection::ComponentType& type);
	bool End(Scene& scene) override;
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	std::string GetName() const override;
	size_t GetSize() const override;
private:
	Uuid m_Entity;
	Reflection::ComponentValues m_Before;
	Reflection::ComponentValues m_After;
};
