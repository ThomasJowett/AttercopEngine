#include "HistoryCommands.h"
#include "Scene/SceneSerializer.h"
#include "Scene/SceneGraph.h"
#include "Reflection/ComponentType.h"
#include "Logging/Logger.h"

#include "TinyXml2/tinyxml2.h"

namespace HistoryCommands
{
	Entity Find(Scene& scene, const Uuid& id)
	{
		Entity entity = scene.GetEntityByID(id);
		if (!entity)
			CLIENT_WARN("Undo can't find entity {0}", id.to_string());
		return entity;
	}

	Entity FindOptional(Scene& scene, const std::optional<Uuid>& id)
	{
		return id ? Find(scene, *id) : Entity();
	}

	static size_t SizeOf(const Reflection::Value& value)
	{
		size_t size = sizeof(value);
		if (const std::string* text = value.TryAs<std::string>())
			size += text->capacity();
		else if (const Reflection::AssetValue* asset = value.TryAs<Reflection::AssetValue>())
			size += asset->path.native().capacity() * sizeof(std::filesystem::path::value_type);
		else if (const Reflection::ValueList* list = value.TryAs<Reflection::ValueList>())
		{
			for (const Reflection::Value& item : *list)
				size += SizeOf(item);
		}
		return size;
	}

	size_t SizeOf(const Reflection::ComponentValues& values)
	{
		size_t size = sizeof(values) + values.custom.capacity();
		for (const Reflection::Value& value : values.values)
			size += SizeOf(value);
		return size;
	}

	static std::optional<Uuid> ParentID(Entity entity)
	{
		if (Entity parent = entity.GetParent())
			return parent.GetID();
		return std::nullopt;
	}

	static std::optional<Uuid> NextSiblingID(Entity entity)
	{
		if (const HierarchyComponent* hierarchyComp = entity.TryGetComponent<HierarchyComponent>())
		{
			if (hierarchyComp->nextSibling != entt::null)
				return Entity(hierarchyComp->nextSibling, entity.GetScene()).GetID();
		}
		return std::nullopt;
	}

	static std::optional<Uuid> PreviousSiblingID(Entity entity)
	{
		if (const HierarchyComponent* hierarchyComp = entity.TryGetComponent<HierarchyComponent>())
		{
			if (hierarchyComp->previousSibling != entt::null)
				return Entity(hierarchyComp->previousSibling, entity.GetScene()).GetID();
		}
		return std::nullopt;
	}
}

using namespace HistoryCommands;

AddEntityCommand::AddEntityCommand(Entity entity)
	: m_Entity(entity.GetID()), m_Parent(ParentID(entity))
{
}

void AddEntityCommand::Undo(Scene& scene)
{
	if (Entity entity = Find(scene, m_Entity))
		SceneGraph::Remove(entity);
}

void AddEntityCommand::Redo(Scene& scene)
{
	Entity entity = SceneSerializer::DeserializeEntity(&scene, m_EntityPrefab, false);
	if (Entity parent = FindOptional(scene, m_Parent))
		SceneGraph::Reparent(entity, parent);
}

bool AddEntityCommand::End(Scene& scene)
{
	Entity entity = Find(scene, m_Entity);
	if (!entity)
		return false;
	m_EntityPrefab = SceneSerializer::SerializeEntity(entity);
	m_Name = entity.GetName();
	return true;
}

RemoveEntityCommand::RemoveEntityCommand(Entity entity)
	: m_Entity(entity.GetID()), m_Parent(ParentID(entity)), m_NextSibling(NextSiblingID(entity)),
	m_EntityPrefab(SceneSerializer::SerializeEntity(entity)), m_Name(entity.GetName())
{
}

void RemoveEntityCommand::Undo(Scene& scene)
{
	Entity entity = SceneSerializer::DeserializeEntity(&scene, m_EntityPrefab, false);
	if (Entity nextSibling = FindOptional(scene, m_NextSibling))
		SceneGraph::MoveBefore(entity, nextSibling);
	else if (Entity parent = FindOptional(scene, m_Parent))
		SceneGraph::Reparent(entity, parent);
}

void RemoveEntityCommand::Redo(Scene& scene)
{
	if (Entity entity = Find(scene, m_Entity))
		SceneGraph::Remove(entity);
}

ReparentEntityCommand::ReparentEntityCommand(Entity entity, Entity newParent)
	: m_Entity(entity.GetID()), m_OldParent(ParentID(entity)), m_OldNextSibling(NextSiblingID(entity)), m_Name(entity.GetName())
{
	if (newParent)
		m_NewParent = newParent.GetID();
}

void ReparentEntityCommand::Undo(Scene& scene)
{
	Entity entity = Find(scene, m_Entity);
	if (!entity)
		return;

	if (Entity nextSibling = FindOptional(scene, m_OldNextSibling))
		SceneGraph::MoveBefore(entity, nextSibling);
	else if (Entity oldParent = FindOptional(scene, m_OldParent))
		SceneGraph::Reparent(entity, oldParent);
	else
		SceneGraph::Unparent(entity);
}

void ReparentEntityCommand::Redo(Scene& scene)
{
	Entity entity = Find(scene, m_Entity);
	if (!entity)
		return;

	if (Entity newParent = FindOptional(scene, m_NewParent))
		SceneGraph::Reparent(entity, newParent);
	else
		SceneGraph::Unparent(entity);
}

MoveSiblingCommand::MoveSiblingCommand(Entity entity, Entity newAnchor, bool newIsBefore)
	: m_Entity(entity.GetID()), m_OldParent(ParentID(entity)), m_OldPreviousSibling(PreviousSiblingID(entity)),
	m_OldNextSibling(NextSiblingID(entity)), m_NewAnchor(newAnchor.GetID()), m_NewIsBefore(newIsBefore), m_Name(entity.GetName())
{
}

void MoveSiblingCommand::Undo(Scene& scene)
{
	Entity entity = Find(scene, m_Entity);
	if (!entity)
		return;

	if (Entity previousSibling = FindOptional(scene, m_OldPreviousSibling))
		SceneGraph::MoveAfter(entity, previousSibling);
	else if (Entity nextSibling = FindOptional(scene, m_OldNextSibling))
		SceneGraph::MoveBefore(entity, nextSibling);
	else if (Entity oldParent = FindOptional(scene, m_OldParent))
		SceneGraph::Reparent(entity, oldParent);
	else
		SceneGraph::Unparent(entity);
}

void MoveSiblingCommand::Redo(Scene& scene)
{
	Entity entity = Find(scene, m_Entity);
	Entity anchor = Find(scene, m_NewAnchor);
	if (!entity || !anchor)
		return;

	if (m_NewIsBefore)
		SceneGraph::MoveBefore(entity, anchor);
	else
		SceneGraph::MoveAfter(entity, anchor);
}

EditKey ComponentEditKey(Entity entity, const Reflection::ComponentType& type)
{
	return EditKey({ entity.GetID() }, &type);
}

static bool SameValues(const Reflection::ComponentValues& a, const Reflection::ComponentValues& b)
{
	return a.values == b.values && a.custom == b.custom;
}

AddComponentCommand::AddComponentCommand(Entity entity, const Reflection::ComponentType& type)
	: m_Entity(entity.GetID())
{
	m_Added.type = &type;
}

void AddComponentCommand::Undo(Scene& scene)
{
	if (Entity entity = HistoryCommands::Find(scene, m_Entity))
		m_Added.type->remove(scene.GetRegistry(), entity.GetHandle());
}

void AddComponentCommand::Redo(Scene& scene)
{
	if (Entity entity = HistoryCommands::Find(scene, m_Entity))
		Reflection::Restore(entity, m_Added, Reflection::ChangeSource::Undo);
}

bool AddComponentCommand::End(Scene& scene)
{
	Entity entity = HistoryCommands::Find(scene, m_Entity);
	if (!entity || !m_Added.type->tryGet(scene.GetRegistry(), entity.GetHandle()))
		return false;
	m_Added = Reflection::Capture(entity, *m_Added.type);
	return true;
}

std::string AddComponentCommand::GetName() const { return "Add " + m_Added.type->name; }
size_t AddComponentCommand::GetSize() const { return sizeof(*this) + HistoryCommands::SizeOf(m_Added); }

RemoveComponentCommand::RemoveComponentCommand(Entity entity, const Reflection::ComponentType& type)
	: m_Entity(entity.GetID()), m_Removed(Reflection::Capture(entity, type))
{
}

void RemoveComponentCommand::Undo(Scene& scene)
{
	if (Entity entity = HistoryCommands::Find(scene, m_Entity))
		Reflection::Restore(entity, m_Removed, Reflection::ChangeSource::Undo);
}

void RemoveComponentCommand::Redo(Scene& scene)
{
	if (Entity entity = HistoryCommands::Find(scene, m_Entity))
		m_Removed.type->remove(scene.GetRegistry(), entity.GetHandle());
}

std::string RemoveComponentCommand::GetName() const { return "Remove " + m_Removed.type->name; }
size_t RemoveComponentCommand::GetSize() const { return sizeof(*this) + HistoryCommands::SizeOf(m_Removed); }

EditComponentCommand::EditComponentCommand(Entity entity, const Reflection::ComponentType& type)
	: m_Entity(entity.GetID()), m_Before(Reflection::Capture(entity, type))
{
}

bool EditComponentCommand::End(Scene& scene)
{
	Entity entity = HistoryCommands::Find(scene, m_Entity);
	if (!entity || !m_Before.type->tryGet(scene.GetRegistry(), entity.GetHandle()))
		return false;
	m_After = Reflection::Capture(entity, *m_Before.type);
	return !SameValues(m_Before, m_After);
}

void EditComponentCommand::Undo(Scene& scene)
{
	if (Entity entity = HistoryCommands::Find(scene, m_Entity))
		Reflection::Restore(entity, m_Before, Reflection::ChangeSource::Undo);
}

void EditComponentCommand::Redo(Scene& scene)
{
	if (Entity entity = HistoryCommands::Find(scene, m_Entity))
		Reflection::Restore(entity, m_After, Reflection::ChangeSource::Undo);
}

std::string EditComponentCommand::GetName() const { return "Edit " + m_Before.type->name; }

size_t EditComponentCommand::GetSize() const
{
	return sizeof(*this) + HistoryCommands::SizeOf(m_Before) + HistoryCommands::SizeOf(m_After);
}
