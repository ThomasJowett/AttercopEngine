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

	static std::string SaveCustomField(const Reflection::FieldInfo& field, const void* component)
	{
		tinyxml2::XMLDocument document;
		tinyxml2::XMLElement* element = document.NewElement("Component");
		document.InsertEndChild(element);
		field.save(component, element);
		tinyxml2::XMLPrinter printer(nullptr, true);
		document.Print(&printer);
		return printer.CStr();
	}

	bool SameSavedValues(const Reflection::ComponentType& type, const void* a, const void* b)
	{
		for (const Reflection::FieldInfo& field : type.fields)
		{
			if (!field.IsSaved())
				continue;
			if (field.IsCustom())
			{
				if (SaveCustomField(field, a) != SaveCustomField(field, b))
					return false;
			}
			else if (!(field.get(a) == field.get(b)))
			{
				return false;
			}
		}
		return true;
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
