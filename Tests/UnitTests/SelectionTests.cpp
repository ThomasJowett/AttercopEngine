#include "vendor/doctest.h"

#include "History/HistoryCommands.h"
#include "History/SceneHistory.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneGraph.h"
#include "Scene/SceneSerializer.h"
#include "Selection/EntitySelection.h"
#include "Selection/SelectionOperations.h"

#include <string>
#include <vector>

namespace SelectionTests
{
	// The hierarchy as names, children in brackets: "A(B C(D)) E"
	std::string Structure(Entity entity)
	{
		std::string text = entity.GetName();
		std::vector<Entity> children = SceneGraph::GetChildren(entity);
		for (size_t i = 0; i < children.size(); ++i)
			text += (i == 0 ? "(" : " ") + Structure(children[i]) + (i + 1 == children.size() ? ")" : "");
		return text;
	}

	std::string Structure(Scene& scene)
	{
		std::string text;
		for (Entity root : SceneGraph::GetRoots(scene))
			text += (text.empty() ? "" : " ") + Structure(root);
		return text;
	}

	std::vector<Uuid> Ids(const std::vector<Entity>& entities)
	{
		std::vector<Uuid> ids;
		for (Entity entity : entities)
			ids.push_back(entity.GetID());
		return ids;
	}

	// A selection and a history that stores it
	struct Editing
	{
		Scene scene{ "" };
		SceneHistory history;
		EntitySelection selection;
		Editing() { history.SetSelection(&selection); }
	};
}

using namespace SelectionTests;

TEST_CASE("Selecting, adding, toggling and removing keep the order and the active entity")
{
	Uuid a(1, 1), b(2, 2), c(3, 3);
	EntitySelection selection;
	CHECK(selection.IsEmpty());
	CHECK(selection.GetActive().IsNil());

	selection.Select(a);
	CHECK(selection.GetAll() == std::vector<Uuid>{ a });
	CHECK(selection.GetActive() == a);

	selection.Add(b);
	selection.Add(c);
	CHECK(selection.GetAll() == std::vector<Uuid>{ a, b, c });
	CHECK(selection.GetActive() == c);
	CHECK(selection.Count() == 3);
	CHECK(selection.Contains(b));

	// Adding what is already there makes it active without repeating it
	selection.Add(a);
	CHECK(selection.GetAll() == std::vector<Uuid>{ a, b, c });
	CHECK(selection.GetActive() == a);

	// Removing the active entity makes the most recently selected remaining one active
	selection.Remove(a);
	CHECK(selection.GetAll() == std::vector<Uuid>{ b, c });
	CHECK(selection.GetActive() == c);
	selection.Remove(b);
	CHECK(selection.GetActive() == c);

	selection.Toggle(c);
	CHECK(selection.IsEmpty());
	CHECK(selection.GetActive().IsNil());
	selection.Toggle(b);
	CHECK(selection.GetActive() == b);

	selection.Select(a);
	CHECK(selection.GetAll() == std::vector<Uuid>{ a });
	selection.Clear();
	CHECK(selection.IsEmpty());
	CHECK_FALSE(selection.Contains(a));
}

TEST_CASE("SetAll keeps the order, drops repeats and takes the given active entity")
{
	Uuid a(1, 1), b(2, 2), c(3, 3);
	EntitySelection selection;
	selection.SetAll({ a, b, a, c }, b);
	CHECK(selection.GetAll() == std::vector<Uuid>{ a, b, c });
	CHECK(selection.GetActive() == b);

	// An active entity that isn't in the list falls back to the last
	selection.SetAll({ a, b }, c);
	CHECK(selection.GetActive() == b);
}

TEST_CASE("The version changes on every edit")
{
	Uuid a(1, 1);
	EntitySelection selection;
	uint64_t version = selection.GetVersion();
	selection.Select(a);
	CHECK(selection.GetVersion() != version);
	version = selection.GetVersion();
	selection.Toggle(a);
	CHECK(selection.GetVersion() != version);
}

TEST_CASE("Resolve skips a deleted entity and finds it again after undo")
{
	RegisterComponents();
	Editing editing;
	Entity first = editing.scene.CreateEntity("First");
	Entity second = editing.scene.CreateEntity("Second");
	const Uuid secondId = second.GetID();
	editing.selection.SetAll({ first.GetID(), secondId });
	CHECK(editing.selection.Resolve(editing.scene).size() == 2);

	// Delete keeps the selection's UUIDs only for what is still there
	editing.selection.Select(secondId);
	SelectionOperations::Delete(editing.history, editing.selection, editing.scene);
	CHECK_FALSE(editing.scene.GetEntityByID(secondId));
	editing.selection.Add(first.GetID());
	editing.selection.Add(secondId);
	CHECK(editing.selection.Resolve(editing.scene) == std::vector<Entity>{ first });

	editing.history.Undo(editing.scene);
	CHECK(editing.scene.GetEntityByID(secondId));
	CHECK(editing.selection.GetAll() == std::vector<Uuid>{ secondId });
	CHECK(editing.selection.Resolve(editing.scene).size() == 1);
}

TEST_CASE("GetTopLevel leaves out an entity whose ancestor is selected")
{
	Scene scene("");
	Entity parent = scene.CreateEntity("Parent");
	Entity child = scene.CreateEntity("Child");
	Entity other = scene.CreateEntity("Other");
	parent.AddChild(child);

	EntitySelection selection;
	selection.SetAll({ child.GetID(), other.GetID(), parent.GetID() });
	CHECK(Ids(selection.GetTopLevel(scene)) == std::vector<Uuid>{ other.GetID(), parent.GetID() });

	// A child alone is top-level
	selection.Select(child.GetID());
	CHECK(Ids(selection.GetTopLevel(scene)) == std::vector<Uuid>{ child.GetID() });
}

TEST_CASE("Invert selects what wasn't selected")
{
	Scene scene("");
	Entity a = scene.CreateEntity("A");
	Entity b = scene.CreateEntity("B");
	Entity c = scene.CreateEntity("C");
	EntitySelection selection;
	selection.Select(b.GetID());
	selection.Invert(Ids(HistoryCommands::AllInHierarchyOrder(scene)));
	CHECK(selection.GetAll() == std::vector<Uuid>{ a.GetID(), c.GetID() });
	CHECK(selection.GetActive() == c.GetID());

	selection.Invert(Ids(HistoryCommands::AllInHierarchyOrder(scene)));
	CHECK(selection.GetAll() == std::vector<Uuid>{ b.GetID() });
}

TEST_CASE("Deleting a selected parent and child deletes the subtree once, and undo restores both")
{
	RegisterComponents();
	Editing editing;
	Entity parent = editing.scene.CreateEntity("Parent");
	Entity child = editing.scene.CreateEntity("Child");
	Entity other = editing.scene.CreateEntity("Other");
	Entity keep = editing.scene.CreateEntity("Keep");
	parent.AddChild(child);
	const Uuid parentId = parent.GetID();
	const Uuid childId = child.GetID();
	const Uuid otherId = other.GetID();

	editing.selection.SetAll({ childId, otherId, parentId }, otherId);
	SelectionOperations::Delete(editing.history, editing.selection, editing.scene);
	CHECK(Structure(editing.scene) == "Keep");
	CHECK(editing.selection.IsEmpty());
	CHECK(editing.history.GetStepCount() == 1);

	editing.history.Undo(editing.scene);
	CHECK(Structure(editing.scene) == "Parent(Child) Other Keep");
	CHECK(editing.scene.GetEntityByID(childId));
	CHECK(editing.selection.GetAll() == std::vector<Uuid>{ childId, otherId, parentId });
	CHECK(editing.selection.GetActive() == otherId);

	editing.history.Redo(editing.scene);
	CHECK(Structure(editing.scene) == "Keep");
	CHECK(editing.selection.IsEmpty());
	(void)keep;
}

TEST_CASE("Duplicating keeps the order and the parents, selects the copies and is one step")
{
	RegisterComponents();
	Editing editing;
	Entity parent = editing.scene.CreateEntity("Parent");
	Entity a = editing.scene.CreateEntity("A");
	Entity b = editing.scene.CreateEntity("B");
	Entity last = editing.scene.CreateEntity("Last");
	parent.AddChild(a);
	parent.AddChild(b);
	b.AddChild(editing.scene.CreateEntity("Inner"));
	const Uuid aId = a.GetID();
	const Uuid bId = b.GetID();

	editing.selection.SetAll({ bId, aId }, aId);
	std::vector<Entity> copies = SelectionOperations::Duplicate(editing.history, editing.selection, editing.scene);
	REQUIRE(copies.size() == 2);
	// Handles don't survive undo, UUIDs do
	const std::vector<Uuid> copyIds = Ids(copies);
	CHECK(Structure(editing.scene) == "Parent(A A_copy B(Inner) B_copy(Inner)) Last");
	CHECK(editing.history.GetStepCount() == 1);

	// Copies in hierarchy order, the active copy matching the active original
	CHECK(editing.selection.GetAll() == copyIds);
	CHECK(editing.selection.GetActive() == copyIds[0]);
	CHECK(copies[0].GetName() == "A_copy");

	editing.history.Undo(editing.scene);
	CHECK(Structure(editing.scene) == "Parent(A B(Inner)) Last");
	CHECK(editing.selection.GetAll() == std::vector<Uuid>{ bId, aId });
	CHECK(editing.selection.GetActive() == aId);

	editing.history.Redo(editing.scene);
	CHECK(Structure(editing.scene) == "Parent(A A_copy B(Inner) B_copy(Inner)) Last");
	CHECK(editing.selection.GetAll() == copyIds);
	(void)last;
}

TEST_CASE("Copy then paste keeps the internal hierarchy, with new UUIDs, at the root")
{
	RegisterComponents();
	Editing editing;
	Entity parent = editing.scene.CreateEntity("Parent");
	Entity child = editing.scene.CreateEntity("Child");
	Entity other = editing.scene.CreateEntity("Other");
	parent.AddChild(child);
	editing.selection.SetAll({ other.GetID(), child.GetID(), parent.GetID() });

	const std::string text = SelectionOperations::Copy(editing.selection, editing.scene);
	CHECK(text.find("<Entities") != std::string::npos);

	std::vector<Entity> pasted = SelectionOperations::Paste(editing.history, editing.selection, editing.scene, text);
	REQUIRE(pasted.size() == 2);
	const std::vector<Uuid> pastedIds = Ids(pasted);
	CHECK(Structure(editing.scene) == "Parent(Child) Other Parent(Child) Other");
	CHECK(pastedIds[0] != parent.GetID());
	CHECK(pasted[0].GetChild().GetID() != child.GetID());
	CHECK(editing.selection.GetAll() == pastedIds);
	CHECK(editing.history.GetStepCount() == 1);

	editing.history.Undo(editing.scene);
	CHECK(Structure(editing.scene) == "Parent(Child) Other");
	CHECK(editing.selection.GetAll().size() == 3);

	editing.history.Redo(editing.scene);
	CHECK(Structure(editing.scene) == "Parent(Child) Other Parent(Child) Other");
	CHECK(editing.scene.GetEntityByID(pastedIds[0]));
}

TEST_CASE("Paste still takes a single copied entity, and ignores other text")
{
	RegisterComponents();
	Editing editing;
	Entity parent = editing.scene.CreateEntity("Parent");
	parent.AddChild(editing.scene.CreateEntity("Child"));

	// What 0.0.3 copied
	const std::string single = SceneSerializer::SerializeEntity(parent);
	std::vector<Entity> pasted = SelectionOperations::Paste(editing.history, editing.selection, editing.scene, single);
	REQUIRE(pasted.size() == 1);
	CHECK(Structure(editing.scene) == "Parent(Child) Parent(Child)");

	CHECK(SelectionOperations::Paste(editing.history, editing.selection, editing.scene, "not xml").empty());
	CHECK(SelectionOperations::Paste(editing.history, editing.selection, editing.scene, "<Other/>").empty());
	CHECK(editing.history.GetStepCount() == 1);
	CHECK(SelectionOperations::Copy(EntitySelection(), editing.scene).empty());
}

TEST_CASE("Dropping many moves them in order as one step, and a drop onto a descendant is refused")
{
	RegisterComponents();
	Editing editing;
	Entity a = editing.scene.CreateEntity("A");
	Entity b = editing.scene.CreateEntity("B");
	Entity c = editing.scene.CreateEntity("C");
	Entity target = editing.scene.CreateEntity("Target");
	Entity inner = editing.scene.CreateEntity("Inner");
	a.AddChild(inner);
	using Position = SelectionOperations::DropPosition;

	// The payload lists them out of order; the drop keeps hierarchy order
	CHECK(SelectionOperations::Move(editing.history, editing.scene, { c.GetID(), a.GetID() }, target, Position::Onto));
	CHECK(Structure(editing.scene) == "B Target(A(Inner) C)");
	CHECK(editing.history.GetStepCount() == 1);

	// Onto the dragged entity's own descendant
	CHECK_FALSE(SelectionOperations::Move(editing.history, editing.scene, { a.GetID() }, inner, Position::Onto));
	CHECK_FALSE(SelectionOperations::Move(editing.history, editing.scene, { a.GetID() }, inner, Position::Before));
	CHECK(editing.history.GetStepCount() == 1);

	editing.history.Undo(editing.scene);
	CHECK(Structure(editing.scene) == "A(Inner) B C Target");

	CHECK(SelectionOperations::Move(editing.history, editing.scene, { a.GetID(), c.GetID() }, target, Position::After));
	CHECK(Structure(editing.scene) == "B Target A(Inner) C");
	CHECK(SelectionOperations::Move(editing.history, editing.scene, { target.GetID() }, c, Position::Before));
	CHECK(Structure(editing.scene) == "B A(Inner) Target C");

	// To the root, in order
	CHECK(SelectionOperations::Move(editing.history, editing.scene, { inner.GetID() }, Entity(), Position::Onto));
	CHECK(Structure(editing.scene) == "B A Target C Inner");
}

TEST_CASE("Hiding the selection hides them all as one step, and shows them when all are hidden")
{
	RegisterComponents();
	Editing editing;
	Entity a = editing.scene.CreateEntity("A");
	Entity b = editing.scene.CreateEntity("B");
	b.AddComponent<HiddenComponent>().hidden = true;
	auto isHidden = [](Entity entity)
	{
		const HiddenComponent* hiddenComp = entity.TryGetComponent<HiddenComponent>();
		return hiddenComp && hiddenComp->hidden;
	};

	// One visible, so both are hidden
	editing.selection.SetAll({ a.GetID(), b.GetID() });
	SelectionOperations::ToggleHidden(editing.history, editing.selection, editing.scene);
	CHECK(isHidden(a));
	CHECK(isHidden(b));
	CHECK(editing.history.GetStepCount() == 1);

	SelectionOperations::ToggleHidden(editing.history, editing.selection, editing.scene);
	CHECK_FALSE(isHidden(a));
	CHECK_FALSE(isHidden(b));
	CHECK(editing.history.GetStepCount() == 2);

	editing.history.Undo(editing.scene);
	editing.history.Undo(editing.scene);
	CHECK_FALSE(a.HasComponent<HiddenComponent>());
	CHECK(isHidden(b));
}
