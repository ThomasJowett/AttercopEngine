#pragma once
#include "History/SceneCommand.h"
#include "Scene/Entity.h"
#include "Scene/Components/TilemapComponent.h"
#include "Reflection/Reflection.h"
#include "Core/UUID.h"

#include <optional>

namespace HistoryCommands
{
	// The entity with this UUID, or a null entity and a warning
	Entity Find(Scene& scene, const Uuid& id);
	// The entity with this UUID, or a null entity if there's no UUID
	Entity FindOptional(Scene& scene, const std::optional<Uuid>& id);
	// Whether two components of one type have the same saved values
	bool SameSavedValues(const Reflection::ComponentType& type, const void* a, const void* b);
}

// The key of an edit of one entity's whole component
template<typename T>
EditKey ComponentEditKey(Entity entity)
{
	return EditKey({ entity.GetID() }, &Reflection::TypeOf<T>());
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

// Bytes a component copy holds outside itself
template<typename T>
size_t ComponentHeapSize(const T& component)
{
	if constexpr (std::is_same_v<T, TilemapComponent>)
	{
		size_t size = component.tiles.capacity() * sizeof(std::vector<uint32_t>);
		for (const std::vector<uint32_t>& row : component.tiles)
			size += row.capacity() * sizeof(uint32_t);
		return size;
	}
	else
	{
		(void)component;
		return 0;
	}
}

template<typename T>
class AddComponentCommand : public SceneCommand
{
public:
	// Construct before adding the component
	explicit AddComponentCommand(Entity entity)
		: m_Entity(entity.GetID())
	{
	}

	void Undo(Scene& scene) override
	{
		if (Entity entity = HistoryCommands::Find(scene, m_Entity))
			entity.RemoveComponent<T>();
	}
	void Redo(Scene& scene) override
	{
		if (Entity entity = HistoryCommands::Find(scene, m_Entity))
			entity.AssignComponent<T>(m_NewComponent);
	}
	bool End(Scene& scene) override
	{
		Entity entity = HistoryCommands::Find(scene, m_Entity);
		if (!entity || !entity.HasComponent<T>())
			return false;
		m_NewComponent = entity.GetComponent<T>();
		return true;
	}
	std::string GetName() const override { return "Add " + Reflection::TypeOf<T>().name; }
	size_t GetSize() const override { return sizeof(*this) + ComponentHeapSize(m_NewComponent); }
private:
	Uuid m_Entity;
	T m_NewComponent;
};

template<typename T>
class RemoveComponentCommand : public SceneCommand
{
public:
	// Construct before removing the component
	explicit RemoveComponentCommand(Entity entity)
		: m_Entity(entity.GetID()), m_RemovedComponent(entity.GetComponent<T>())
	{
	}

	void Undo(Scene& scene) override
	{
		if (Entity entity = HistoryCommands::Find(scene, m_Entity))
			entity.AssignComponent<T>(m_RemovedComponent);
	}
	void Redo(Scene& scene) override
	{
		if (Entity entity = HistoryCommands::Find(scene, m_Entity))
			entity.RemoveComponent<T>();
	}
	std::string GetName() const override { return "Remove " + Reflection::TypeOf<T>().name; }
	size_t GetSize() const override { return sizeof(*this) + ComponentHeapSize(m_RemovedComponent); }

private:
	Uuid m_Entity;
	T m_RemovedComponent;
};

// Copies of a whole component before and after an edit
template<typename T>
class EditComponentCommand : public SceneCommand
{
public:
	// Construct before the edit
	explicit EditComponentCommand(Entity entity)
		: m_Entity(entity.GetID()), m_OldComponent(entity.GetComponent<T>())
	{
	}

	bool End(Scene& scene) override
	{
		Entity entity = HistoryCommands::Find(scene, m_Entity);
		if (!entity || !entity.HasComponent<T>())
			return false;
		m_NewComponent = entity.GetComponent<T>();
		return !HistoryCommands::SameSavedValues(Reflection::TypeOf<T>(), &m_OldComponent, &m_NewComponent);
	}

	void Undo(Scene& scene) override
	{
		if (Entity entity = HistoryCommands::Find(scene, m_Entity))
			entity.AssignComponent<T>(m_OldComponent);
	}

	void Redo(Scene& scene) override
	{
		if (Entity entity = HistoryCommands::Find(scene, m_Entity))
			entity.AssignComponent<T>(m_NewComponent);
	}

	std::string GetName() const override { return "Edit " + Reflection::TypeOf<T>().name; }
	size_t GetSize() const override
	{
		return sizeof(*this) + ComponentHeapSize(m_OldComponent) + ComponentHeapSize(m_NewComponent);
	}

private:
	Uuid m_Entity;
	T m_OldComponent;
	T m_NewComponent;
};
