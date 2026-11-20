#pragma once
#include "History/SceneCommand.h"
#include "Scene/Entity.h"
#include "Reflection/Reflection.h"
#include "Core/UUID.h"

#include <optional>

namespace HistoryCommands
{
	// The entity with this UUID, or a null entity and a warning
	Entity Find(Scene& scene, const Uuid& id);
	// The entity with this UUID, or a null entity if there's no UUID
	Entity FindOptional(Scene& scene, const std::optional<Uuid>& id);
	// Bytes held by captured values
	size_t SizeOf(const Reflection::ComponentValues& values);
}

// The key of an edit of one entity's whole component
EditKey ComponentEditKey(Entity entity, const Reflection::ComponentType& type);

template<typename T>
EditKey ComponentEditKey(Entity entity)
{
	return ComponentEditKey(entity, Reflection::TypeOf<T>());
}

class AddEntityCommand : public SceneCommand
{
public:
	// Construct once the entity is complete, including its parent
	explicit AddEntityCommand(Entity entity);
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	bool End(Scene& scene) override;
	std::string GetName() const override { return "Create " + m_Name; }
	size_t GetSize() const override { return sizeof(*this) + m_EntityPrefab.capacity() + m_Name.capacity(); }
private:
	Uuid m_Entity;
	std::optional<Uuid> m_Parent;
	std::string m_EntityPrefab;
	std::string m_Name;
};

class RemoveEntityCommand : public SceneCommand
{
public:
	// Construct before the entity is removed
	explicit RemoveEntityCommand(Entity entity);
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	std::string GetName() const override { return "Delete " + m_Name; }
	size_t GetSize() const override { return sizeof(*this) + m_EntityPrefab.capacity() + m_Name.capacity(); }

private:
	Uuid m_Entity;
	std::optional<Uuid> m_Parent;
	// The sibling or root after it, so undo puts it back in place
	std::optional<Uuid> m_NextSibling;
	std::string m_EntityPrefab;
	std::string m_Name;
};

class ReparentEntityCommand : public SceneCommand
{
public:
	// Construct before reparenting; a null newParent makes it a root
	ReparentEntityCommand(Entity entity, Entity newParent);
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	std::string GetName() const override { return "Reparent " + m_Name; }
	size_t GetSize() const override { return sizeof(*this) + m_Name.capacity(); }

private:
	Uuid m_Entity;
	std::optional<Uuid> m_NewParent;
	std::optional<Uuid> m_OldParent;
	// The sibling or root it was before, so undo puts it back in place
	std::optional<Uuid> m_OldNextSibling;
	std::string m_Name;
};

class MoveSiblingCommand : public SceneCommand
{
public:
	// Construct before SceneGraph::MoveBefore or MoveAfter; newAnchor and newIsBefore describe that move
	MoveSiblingCommand(Entity entity, Entity newAnchor, bool newIsBefore);
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	std::string GetName() const override { return "Move " + m_Name; }
	size_t GetSize() const override { return sizeof(*this) + m_Name.capacity(); }

private:
	Uuid m_Entity;
	std::optional<Uuid> m_OldParent;
	std::optional<Uuid> m_OldPreviousSibling;
	std::optional<Uuid> m_OldNextSibling;
	Uuid m_NewAnchor;
	bool m_NewIsBefore;
	std::string m_Name;
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
