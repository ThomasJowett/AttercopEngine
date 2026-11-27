#include "SelectionOperations.h"

#include "History/HistoryCommands.h"
#include "History/PropertyCommands.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneGraph.h"
#include "Scene/SceneSerializer.h"

namespace SelectionOperations
{
	std::vector<Entity> GetRoots(Scene& scene, const EntitySelection& selection)
	{
		return HistoryCommands::TopLevelInOrder(selection.Resolve(scene));
	}

	void Delete(SceneHistory& history, EntitySelection& selection, Scene& scene)
	{
		std::vector<Entity> roots = GetRoots(scene, selection);
		if (roots.empty())
			return;

		// An edit still open on them is recorded before they go
		history.End(scene);
		const SelectionState before = selection.Capture();
		Scope<DeleteEntitiesCommand> command = CreateScope<DeleteEntitiesCommand>(roots);
		for (Entity root : roots)
			root.Destroy();
		selection.Clear();
		history.Push(scene, std::move(command), &before);
	}

	std::vector<Entity> Duplicate(SceneHistory& history, EntitySelection& selection, Scene& scene)
	{
		std::vector<Entity> roots = GetRoots(scene, selection);
		if (roots.empty())
			return {};

		const SelectionState before = selection.Capture();
		// One copy of them all, so references between them point at the copies
		std::vector<Entity> copies = scene.DuplicateEntities(roots);
		std::vector<Uuid> ids;
		Uuid active = Uuid::Nil();
		for (size_t i = 0; i < copies.size(); ++i)
		{
			Entity copy = copies[i];
			Entity root = roots[i];
			copy.GetName() += "_copy";
			// Right after its original
			if (copy.GetParent().GetHandle() == root.GetParent().GetHandle())
				SceneGraph::MoveAfter(copy, root);
			ids.push_back(copy.GetID());
			if (root.GetID() == before.active)
				active = copy.GetID();
		}
		selection.SetAll(std::move(ids), active);
		history.Push(scene, CreateScope<CreateEntitiesCommand>(copies), &before);
		return copies;
	}

	std::string Copy(const EntitySelection& selection, Scene& scene)
	{
		std::vector<Entity> roots = GetRoots(scene, selection);
		if (roots.empty())
			return {};
		return SceneSerializer::SerializeEntities(roots);
	}

	std::vector<Entity> Paste(SceneHistory& history, EntitySelection& selection, Scene& scene, const std::string& text)
	{
		// New UUIDs now, which redo keeps; references between the pasted entities point at them
		const SelectionState before = selection.Capture();
		std::vector<Entity> pasted = SceneSerializer::DeserializeEntities(&scene, text, true);
		if (pasted.empty())
			return pasted;

		std::vector<Uuid> ids;
		for (Entity root : pasted)
			ids.push_back(root.GetID());
		selection.SetAll(std::move(ids));
		history.Push(scene, CreateScope<CreateEntitiesCommand>(pasted), &before);
		return pasted;
	}

	void ToggleHidden(SceneHistory& history, const EntitySelection& selection, Scene& scene)
	{
		std::vector<Entity> entities = selection.Resolve(scene);
		if (entities.empty())
			return;

		bool allHidden = true;
		for (Entity entity : entities)
		{
			const HiddenComponent* hiddenComp = entity.TryGetComponent<HiddenComponent>();
			allHidden &= hiddenComp && hiddenComp->hidden;
		}
		HistoryCommands::SetHidden(history, entities, !allHidden);
	}

	// True if entity is ancestor or one of its descendants
	static bool IsSelfOrDescendant(Entity entity, Entity ancestor)
	{
		for (; entity; entity = entity.GetParent())
		{
			if (entity == ancestor)
				return true;
		}
		return false;
	}

	bool Move(SceneHistory& history, Scene& scene, const std::vector<Uuid>& draggedIds, Entity target, DropPosition position)
	{
		if (target && !target.BelongsToScene(&scene))
			return false;

		std::vector<Entity> found;
		for (const Uuid& id : draggedIds)
		{
			if (Entity entity = scene.GetEntityByID(id))
				found.push_back(entity);
		}
		// In hierarchy order, so the drop keeps their order
		std::vector<Entity> dragged = HistoryCommands::TopLevelInOrder(found);
		if (dragged.empty())
			return false;

		// No drops onto a dragged entity or its descendants
		if (target)
		{
			for (Entity entity : dragged)
			{
				if (IsSelfOrDescendant(target, entity))
					return false;
			}
		}

		Scope<ReparentEntitiesCommand> command = CreateScope<ReparentEntitiesCommand>(dragged);
		if (position == DropPosition::Before && target)
		{
			for (Entity entity : dragged)
				SceneGraph::MoveBefore(entity, target);
		}
		else if (position == DropPosition::After && target)
		{
			// Last first, so each lands right after the target and they end up in order
			for (auto entity = dragged.rbegin(); entity != dragged.rend(); ++entity)
				SceneGraph::MoveAfter(*entity, target);
		}
		else
		{
			Entity parent = position == DropPosition::Onto ? target : Entity();
			for (Entity entity : dragged)
			{
				if (entity.GetParent().GetHandle() == parent.GetHandle())
					continue;
				if (parent)
					SceneGraph::Reparent(entity, parent);
				else
					SceneGraph::Unparent(entity);
			}
		}
		history.Push(scene, std::move(command));
		return true;
	}
}
