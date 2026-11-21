#include "HistoryCommands.h"
#include "Scene/SceneSerializer.h"
#include "Scene/SceneGraph.h"
#include "Reflection/ComponentType.h"
#include "Logging/Logger.h"

#include "TinyXml2/tinyxml2.h"

#include <algorithm>
#include <unordered_set>

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

	static std::optional<Uuid> IdOf(Entity entity, entt::entity handle)
	{
		if (handle == entt::null)
			return std::nullopt;
		return Entity(handle, entity.GetScene()).GetID();
	}

	static void AddInOrder(Entity entity, std::vector<Entity>& order)
	{
		order.push_back(entity);
		for (Entity child : SceneGraph::GetChildren(entity))
			AddInOrder(child, order);
	}

	std::vector<Entity> TopLevelInOrder(const std::vector<Entity>& entities)
	{
		if (entities.empty())
			return {};

		Entity first = entities.front();
		Scene& scene = *first.GetScene();
		std::unordered_set<entt::entity> chosen;
		for (Entity entity : entities)
		{
			if (entity.BelongsToScene(&scene))
				chosen.insert(entity.GetHandle());
		}

		std::vector<Entity> order;
		for (Entity root : SceneGraph::GetRoots(scene))
			AddInOrder(root, order);

		std::vector<Entity> topLevel;
		for (Entity entity : order)
		{
			if (!chosen.count(entity.GetHandle()))
				continue;
			bool underChosen = false;
			for (Entity ancestor = entity.GetParent(); ancestor && !underChosen; ancestor = ancestor.GetParent())
				underChosen = chosen.count(ancestor.GetHandle()) > 0;
			if (!underChosen)
				topLevel.push_back(entity);
		}
		return topLevel;
	}

	EntityPosition PositionOf(Entity entity)
	{
		EntityPosition position;
		position.entity = entity.GetID();
		if (const HierarchyComponent* hierarchyComp = entity.TryGetComponent<HierarchyComponent>())
		{
			position.parent = IdOf(entity, hierarchyComp->parent);
			position.nextSibling = IdOf(entity, hierarchyComp->nextSibling);
		}
		return position;
	}

	void MoveTo(Scene& scene, Entity entity, const EntityPosition& position)
	{
		if (Entity parent = FindOptional(scene, position.parent))
		{
			SceneGraph::Reparent(entity, parent);
		}
		else
		{
			// The last root
			SceneGraph::Unparent(entity);
			std::vector<Entity> roots = SceneGraph::GetRoots(scene);
			if (!roots.empty() && roots.back() != entity)
				SceneGraph::MoveAfter(entity, roots.back());
		}

		if (Entity nextSibling = FindOptional(scene, position.nextSibling))
			SceneGraph::MoveBefore(entity, nextSibling);
	}

	EntitySubtree SaveSubtree(Entity entity)
	{
		return { PositionOf(entity), SceneSerializer::SerializeEntity(entity), entity.GetName() };
	}

	void RestoreSubtrees(Scene& scene, const std::vector<EntitySubtree>& subtrees)
	{
		// Last first, so a next sibling that is also restored is already in place
		for (auto subtree = subtrees.rbegin(); subtree != subtrees.rend(); ++subtree)
		{
			if (Entity entity = SceneSerializer::DeserializeEntity(&scene, subtree->serialised, false))
				MoveTo(scene, entity, subtree->position);
		}
	}

	void RemoveSubtrees(Scene& scene, const std::vector<EntitySubtree>& subtrees)
	{
		for (const EntitySubtree& subtree : subtrees)
		{
			if (Entity entity = Find(scene, subtree.position.entity))
				scene.RemoveEntity(entity);
		}
	}

	static size_t SizeOf(const std::vector<EntitySubtree>& subtrees)
	{
		size_t size = subtrees.capacity() * sizeof(EntitySubtree);
		for (const EntitySubtree& subtree : subtrees)
			size += subtree.serialised.capacity() + subtree.name.capacity();
		return size;
	}

	static std::string NameOf(const std::string& action, const std::vector<EntitySubtree>& subtrees)
	{
		if (subtrees.size() == 1)
			return action + " " + subtrees.front().name;
		return action + " " + std::to_string(subtrees.size()) + " Entities";
	}

	static std::vector<Uuid> RootsOf(const std::vector<EntitySubtree>& subtrees)
	{
		std::vector<Uuid> roots;
		for (const EntitySubtree& subtree : subtrees)
			roots.push_back(subtree.position.entity);
		return roots;
	}

	static std::vector<EntityPosition> PositionsInOrder(Scene& scene, const std::vector<Uuid>& ids)
	{
		std::vector<Entity> entities;
		for (const Uuid& id : ids)
		{
			if (Entity entity = Find(scene, id))
				entities.push_back(entity);
		}
		std::vector<EntityPosition> positions;
		for (Entity entity : TopLevelInOrder(entities))
			positions.push_back(PositionOf(entity));
		return positions;
	}

	static void MoveAllTo(Scene& scene, const std::vector<EntityPosition>& positions)
	{
		// Last first, so a next sibling that also moves is already in place
		for (auto position = positions.rbegin(); position != positions.rend(); ++position)
		{
			if (Entity entity = Find(scene, position->entity))
				MoveTo(scene, entity, *position);
		}
	}
}

using namespace HistoryCommands;

CreateEntitiesCommand::CreateEntitiesCommand(const std::vector<Entity>& roots)
{
	for (Entity root : roots)
		m_Roots.push_back(root.GetID());
}

void CreateEntitiesCommand::Undo(Scene& scene)
{
	RemoveSubtrees(scene, m_Subtrees);
}

void CreateEntitiesCommand::Redo(Scene& scene)
{
	RestoreSubtrees(scene, m_Subtrees);
}

bool CreateEntitiesCommand::End(Scene& scene)
{
	std::vector<Entity> roots;
	for (const Uuid& id : m_Roots)
	{
		if (Entity root = Find(scene, id))
			roots.push_back(root);
	}
	m_Subtrees.clear();
	for (Entity root : TopLevelInOrder(roots))
		m_Subtrees.push_back(SaveSubtree(root));
	return !m_Subtrees.empty();
}

std::string CreateEntitiesCommand::GetName() const { return NameOf("Create", m_Subtrees); }

size_t CreateEntitiesCommand::GetSize() const
{
	return sizeof(*this) + m_Roots.capacity() * sizeof(Uuid) + HistoryCommands::SizeOf(m_Subtrees);
}

std::vector<Uuid> CreateEntitiesCommand::GetSelectionAfterRedo() const { return RootsOf(m_Subtrees); }

DeleteEntitiesCommand::DeleteEntitiesCommand(const std::vector<Entity>& roots)
{
	for (Entity root : TopLevelInOrder(roots))
		m_Subtrees.push_back(SaveSubtree(root));
}

void DeleteEntitiesCommand::Undo(Scene& scene)
{
	RestoreSubtrees(scene, m_Subtrees);
}

void DeleteEntitiesCommand::Redo(Scene& scene)
{
	RemoveSubtrees(scene, m_Subtrees);
}

std::string DeleteEntitiesCommand::GetName() const { return NameOf("Delete", m_Subtrees); }
size_t DeleteEntitiesCommand::GetSize() const { return sizeof(*this) + HistoryCommands::SizeOf(m_Subtrees); }
std::vector<Uuid> DeleteEntitiesCommand::GetSelectionAfterUndo() const { return RootsOf(m_Subtrees); }

ReparentEntitiesCommand::ReparentEntitiesCommand(const std::vector<Entity>& entities)
{
	for (Entity entity : TopLevelInOrder(entities))
	{
		m_Entities.push_back(entity.GetID());
		m_Before.push_back(PositionOf(entity));
		m_Name = "Move " + entity.GetName();
	}
	if (m_Entities.size() != 1)
		m_Name = "Move " + std::to_string(m_Entities.size()) + " Entities";
}

void ReparentEntitiesCommand::Undo(Scene& scene)
{
	MoveAllTo(scene, m_Before);
}

void ReparentEntitiesCommand::Redo(Scene& scene)
{
	MoveAllTo(scene, m_After);
}

bool ReparentEntitiesCommand::End(Scene& scene)
{
	m_After = PositionsInOrder(scene, m_Entities);
	for (const EntityPosition& after : m_After)
	{
		if (std::find(m_Before.begin(), m_Before.end(), after) == m_Before.end())
			return true;
	}
	return false;
}

std::string ReparentEntitiesCommand::GetName() const { return m_Name; }

size_t ReparentEntitiesCommand::GetSize() const
{
	return sizeof(*this) + m_Entities.capacity() * sizeof(Uuid) + m_Name.capacity()
		+ (m_Before.capacity() + m_After.capacity()) * sizeof(EntityPosition);
}

CompoundCommand::CompoundCommand(std::string name)
	: m_Name(std::move(name))
{
}

void CompoundCommand::Append(Scope<SceneCommand> command)
{
	if (command)
		m_Commands.push_back(std::move(command));
}

void CompoundCommand::Undo(Scene& scene)
{
	for (auto command = m_Commands.rbegin(); command != m_Commands.rend(); ++command)
		(*command)->Undo(scene);
}

void CompoundCommand::Redo(Scene& scene)
{
	for (Scope<SceneCommand>& command : m_Commands)
		command->Redo(scene);
}

std::string CompoundCommand::GetName() const
{
	if (m_Name.empty() && m_Commands.size() == 1)
		return m_Commands.front()->GetName();
	return m_Name;
}

size_t CompoundCommand::GetSize() const
{
	size_t size = sizeof(*this) + m_Name.capacity() + m_Commands.capacity() * sizeof(Scope<SceneCommand>);
	for (const Scope<SceneCommand>& command : m_Commands)
		size += command->GetSize();
	return size;
}

static void AddUnique(std::vector<Uuid>& ids, const std::vector<Uuid>& more)
{
	for (const Uuid& id : more)
	{
		if (std::find(ids.begin(), ids.end(), id) == ids.end())
			ids.push_back(id);
	}
}

std::vector<Uuid> CompoundCommand::GetSelectionAfterUndo() const
{
	std::vector<Uuid> selection;
	for (auto command = m_Commands.rbegin(); command != m_Commands.rend(); ++command)
		AddUnique(selection, (*command)->GetSelectionAfterUndo());
	return selection;
}

std::vector<Uuid> CompoundCommand::GetSelectionAfterRedo() const
{
	std::vector<Uuid> selection;
	for (const Scope<SceneCommand>& command : m_Commands)
		AddUnique(selection, command->GetSelectionAfterRedo());
	return selection;
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
