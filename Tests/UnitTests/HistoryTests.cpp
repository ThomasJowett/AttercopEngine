#include "vendor/doctest.h"

#include "History/HistoryCommands.h"
#include "History/PropertyCommands.h"
#include "History/SceneHistory.h"
#include "Inspector/MultiEdit.h"
#include "Inspector/PropertyEdits.h"
#include "ComponentFixture.h"
#include "Reflection/Reflection.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneGraph.h"
#include "Scene/SceneSerializer.h"
#include "Selection/EntitySelection.h"
#include "Selection/GizmoTransform.h"

#include "TinyXml2/tinyxml2.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace HistoryTests
{
	Entity CreateMovable(Scene& scene, const std::string& name)
	{
		Entity entity = scene.CreateEntity(name);
		entity.AddComponent<TransformComponent>();
		return entity;
	}

	float X(Entity entity)
	{
		return entity.GetComponent<TransformComponent>().GetPosition().x;
	}

	void SetX(Entity entity, float x)
	{
		entity.GetComponent<TransformComponent>().SetPosition(Vector3f(x, 0.0f, 0.0f));
	}

	Reflection::PropertyPath PropertyAt(const char* text)
	{
		std::optional<Reflection::PropertyPath> path = Reflection::PropertyPath::Parse(text);
		REQUIRE(path);
		return *path;
	}

	// Moves the entity as one pushed step
	void PushMove(SceneHistory& history, Scene& scene, Entity entity, float x)
	{
		Scope<SetPropertyCommand> command = CreateScope<SetPropertyCommand>(std::vector<Entity>{ entity }, PropertyAt("Transform.Position"));
		SetX(entity, x);
		history.Push(scene, std::move(command));
	}

	// Moves the entity inside the interaction's open step, as a drag does each frame
	void DragTo(SceneHistory& history, Entity entity, float x, uint32_t interaction = 1)
	{
		HistoryCommands::RecordProperty(history, interaction, { entity }, PropertyAt("Transform.Position"));
		SetX(entity, x);
	}

	// A step of a given size that logs its undo and redo
	class SizedCommand : public SceneCommand
	{
	public:
		SizedCommand(std::string name, size_t size, std::vector<std::string>& log)
			: m_Name(std::move(name)), m_Size(size), m_Log(log)
		{
		}
		void Undo(Scene&) override { m_Log.push_back("undo " + m_Name); }
		void Redo(Scene&) override { m_Log.push_back("redo " + m_Name); }
		std::string GetName() const override { return m_Name; }
		size_t GetSize() const override { return m_Size; }
	private:
		std::string m_Name;
		size_t m_Size;
		std::vector<std::string>& m_Log;
	};

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

	// Moves the entities as the Hierarchy's drag and drop does, as one pushed step
	template<typename Move>
	void PushReparent(SceneHistory& history, Scene& scene, const std::vector<Entity>& entities, Move move)
	{
		Scope<ReparentEntitiesCommand> command = CreateScope<ReparentEntitiesCommand>(entities);
		move();
		history.Push(scene, std::move(command));
	}

	// A default-constructed component's value of the field, or a custom field's saved XML
	Reflection::Value DefaultOf(const Reflection::ComponentType& type, int field)
	{
		std::shared_ptr<void> component = type.create();
		const Reflection::FieldInfo& info = type.fields[field];
		if (!info.IsCustom())
			return info.get(component.get());
		tinyxml2::XMLDocument document;
		tinyxml2::XMLElement* element = document.NewElement(type.name.c_str());
		document.InsertEndChild(element);
		info.save(component.get(), element);
		tinyxml2::XMLPrinter printer(nullptr, true);
		document.Print(&printer);
		return Reflection::Value(std::string(printer.CStr()));
	}

	bool SameValues(const Reflection::ComponentValues& a, const Reflection::ComponentValues& b)
	{
		return a.values == b.values && a.custom == b.custom;
	}

	// Saving as the editor does: the file matches the scene at this position
	void Save(SceneHistory& history, Scene& scene)
	{
		history.End(scene);
		history.MarkSaved();
		scene.MakeClean();
	}
}

using namespace HistoryTests;

TEST_CASE("Pushed steps undo and redo in order")
{
	RegisterComponents();
	Scene scene("");
	Entity entity = CreateMovable(scene, "Mover");
	SceneHistory history;
	CHECK_FALSE(history.CanUndo());
	CHECK_FALSE(history.CanRedo());

	PushMove(history, scene, entity, 1.0f);
	PushMove(history, scene, entity, 2.0f);
	PushMove(history, scene, entity, 3.0f);
	CHECK(history.GetStepCount() == 3);
	CHECK(history.CanUndo());
	CHECK_FALSE(history.CanRedo());

	history.Undo(scene);
	CHECK(X(entity) == 2.0f);
	history.Undo(scene);
	CHECK(X(entity) == 1.0f);
	CHECK(history.CanRedo());

	history.Redo(scene);
	CHECK(X(entity) == 2.0f);
	history.Undo(scene);
	history.Undo(scene);
	CHECK(X(entity) == 0.0f);
	CHECK_FALSE(history.CanUndo());

	// Nothing left to undo leaves the scene alone
	history.Undo(scene);
	CHECK(X(entity) == 0.0f);
	CHECK(history.GetPosition() == 0);
}

TEST_CASE("A push after undo drops the redo tail")
{
	RegisterComponents();
	Scene scene("");
	Entity entity = CreateMovable(scene, "Mover");
	SceneHistory history;

	PushMove(history, scene, entity, 1.0f);
	PushMove(history, scene, entity, 2.0f);
	PushMove(history, scene, entity, 3.0f);
	history.Undo(scene);
	history.Undo(scene);
	CHECK(history.CanRedo());

	PushMove(history, scene, entity, 5.0f);
	CHECK_FALSE(history.CanRedo());
	CHECK(history.GetStepCount() == 2);

	history.Redo(scene);
	CHECK(X(entity) == 5.0f);
	history.Undo(scene);
	CHECK(X(entity) == 1.0f);
}

TEST_CASE("A step that changed nothing isn't pushed")
{
	RegisterComponents();
	Scene scene("");
	Entity entity = CreateMovable(scene, "Mover");
	SceneHistory history;

	SUBCASE("dragged back to where it started")
	{
		DragTo(history, entity, 4.0f);
		DragTo(history, entity, 0.0f);
		CHECK(history.HasOpenStep());
		history.End(scene);
		CHECK_FALSE(history.HasOpenStep());
		CHECK(history.GetStepCount() == 0);
		CHECK_FALSE(scene.IsDirty());
	}

	SUBCASE("pushed with no change")
	{
		history.Push(scene, CreateScope<SetPropertyCommand>(std::vector<Entity>{ entity }, PropertyAt("Transform.Position")));
		CHECK(history.GetStepCount() == 0);
	}

	SUBCASE("ending with no open step")
	{
		history.End(scene);
		CHECK(history.GetStepCount() == 0);
	}
}

TEST_CASE("One step is open at a time, and a new key closes the old one")
{
	RegisterComponents();
	Scene scene("");
	Entity a = CreateMovable(scene, "A");
	Entity b = CreateMovable(scene, "B");
	SceneHistory history;

	// A drag over many frames is one step, from where it started
	DragTo(history, a, 1.0f, 1);
	DragTo(history, a, 2.0f, 1);
	DragTo(history, a, 3.0f, 1);
	CHECK(history.IsOpen(EditKey::ForInteraction(1)));
	CHECK(history.GetStepCount() == 0);

	// Another interaction is another key
	CHECK_FALSE(history.IsOpen(EditKey::ForInteraction(2)));

	DragTo(history, b, 7.0f, 2);
	CHECK(history.GetStepCount() == 1);
	CHECK_FALSE(history.IsOpen(EditKey::ForInteraction(1)));
	CHECK(history.IsOpen(EditKey::ForInteraction(2)));

	// Undo closes the open step first
	CHECK(history.CanUndo());
	history.Undo(scene);
	CHECK(history.GetStepCount() == 2);
	CHECK(X(b) == 0.0f);
	CHECK(X(a) == 3.0f);

	history.Undo(scene);
	CHECK(X(a) == 0.0f);
	history.Redo(scene);
	CHECK(X(a) == 3.0f);
}

TEST_CASE("Keys match on entities, component, property and interaction")
{
	RegisterComponents();
	const Uuid first, second;
	CHECK(EditKey({ first, second }, nullptr) == EditKey({ second, first }, nullptr));
	CHECK(EditKey({ first }, nullptr, "Position") != EditKey({ first }, nullptr, "Scale"));
	CHECK(EditKey({ first }, nullptr) != EditKey({ second }, nullptr));
	CHECK(EditKey::ForInteraction(3) == EditKey::ForInteraction(3));
	CHECK(EditKey::ForInteraction(3) != EditKey::ForInteraction(4));
	CHECK(EditKey::ForInteraction(3) != EditKey());
}

TEST_CASE("The scene is clean at the saved position")
{
	RegisterComponents();
	Scene scene("");
	Entity entity = CreateMovable(scene, "Mover");
	scene.MakeClean();
	SceneHistory history;
	CHECK(history.IsAtSavedPoint());

	SUBCASE("save, edit, undo back is clean")
	{
		PushMove(history, scene, entity, 1.0f);
		CHECK(scene.IsDirty());
		Save(history, scene);

		PushMove(history, scene, entity, 2.0f);
		CHECK(scene.IsDirty());
		CHECK_FALSE(history.IsAtSavedPoint());

		history.Undo(scene);
		CHECK(history.IsAtSavedPoint());
		CHECK_FALSE(scene.IsDirty());

		history.Undo(scene);
		CHECK(scene.IsDirty());
		history.Redo(scene);
		CHECK_FALSE(scene.IsDirty());
		history.Redo(scene);
		CHECK(scene.IsDirty());
	}

	SUBCASE("undoing every step of an unsaved scene is clean")
	{
		PushMove(history, scene, entity, 1.0f);
		PushMove(history, scene, entity, 2.0f);
		history.Undo(scene);
		history.Undo(scene);
		CHECK_FALSE(scene.IsDirty());
	}

	SUBCASE("save, undo, then a new edit stays dirty")
	{
		PushMove(history, scene, entity, 1.0f);
		PushMove(history, scene, entity, 2.0f);
		Save(history, scene);

		history.Undo(scene);
		CHECK(scene.IsDirty());
		PushMove(history, scene, entity, 3.0f);
		CHECK(scene.IsDirty());

		// The saved step was dropped, so no position is clean
		history.Undo(scene);
		CHECK(scene.IsDirty());
		history.Undo(scene);
		CHECK(scene.IsDirty());
		history.Redo(scene);
		history.Redo(scene);
		CHECK(scene.IsDirty());

		Save(history, scene);
		CHECK(history.IsAtSavedPoint());
	}

	SUBCASE("an unrecorded change stays dirty until the next save")
	{
		PushMove(history, scene, entity, 1.0f);
		Save(history, scene);

		history.MarkUnrecordedChange();
		CHECK_FALSE(history.IsAtSavedPoint());
		PushMove(history, scene, entity, 2.0f);
		history.Undo(scene);
		CHECK(scene.IsDirty());

		Save(history, scene);
		CHECK(history.IsAtSavedPoint());
		PushMove(history, scene, entity, 3.0f);
		history.Undo(scene);
		CHECK_FALSE(scene.IsDirty());
	}

	SUBCASE("saving while a step is open closes it first")
	{
		DragTo(history, entity, 4.0f);
		Save(history, scene);
		CHECK(history.GetStepCount() == 1);
		CHECK(history.IsAtSavedPoint());
		history.Undo(scene);
		CHECK(scene.IsDirty());
	}

	SUBCASE("clearing makes the current scene the saved one")
	{
		PushMove(history, scene, entity, 1.0f);
		history.Clear();
		CHECK(history.IsAtSavedPoint());
		CHECK(history.GetStepCount() == 0);
		CHECK_FALSE(history.CanUndo());
	}
}

TEST_CASE("Pushing past the memory budget drops the oldest steps")
{
	RegisterComponents();
	Scene scene("");
	std::vector<std::string> log;
	SceneHistory history(100);

	history.Push(scene, CreateScope<SizedCommand>("1", 30, log));
	history.Push(scene, CreateScope<SizedCommand>("2", 30, log));
	history.Push(scene, CreateScope<SizedCommand>("3", 30, log));
	CHECK(history.GetStepCount() == 3);
	CHECK(history.GetSize() == 90);

	history.Push(scene, CreateScope<SizedCommand>("4", 30, log));
	CHECK(history.GetStepCount() == 3);
	CHECK(history.GetSize() == 90);
	CHECK(history.GetPosition() == 3);

	while (history.CanUndo())
		history.Undo(scene);
	CHECK(log == std::vector<std::string>{ "undo 4", "undo 3", "undo 2" });

	SUBCASE("the redo tail doesn't count once dropped")
	{
		history.Push(scene, CreateScope<SizedCommand>("5", 30, log));
		CHECK(history.GetStepCount() == 1);
		CHECK(history.GetSize() == 30);
	}

	SUBCASE("the newest step is kept however big")
	{
		history.Redo(scene);
		history.Redo(scene);
		history.Redo(scene);
		history.Push(scene, CreateScope<SizedCommand>("big", 500, log));
		CHECK(history.GetStepCount() == 1);
		CHECK(history.GetSize() == 500);
		CHECK(history.CanUndo());
	}

	SUBCASE("the saved point is lost with the step it was at")
	{
		SceneHistory saved(100);
		saved.MarkSaved();
		for (int i = 0; i < 5; ++i)
			saved.Push(scene, CreateScope<SizedCommand>(std::to_string(i), 30, log));
		while (saved.CanUndo())
			saved.Undo(scene);
		CHECK_FALSE(saved.IsAtSavedPoint());
	}
}

TEST_CASE("Nothing is recorded in Play, and the history still applies after Stop")
{
	RegisterComponents();
	Scene scene("");
	Entity entity = CreateMovable(scene, "Mover");
	const Uuid id = entity.GetID();
	SceneHistory history;

	PushMove(history, scene, entity, 1.0f);
	DragTo(history, entity, 2.0f);

	// What the editor does on Play
	history.End(scene);
	history.SetEnabled(false);
	scene.OnRuntimeStart();
	CHECK(history.GetStepCount() == 2);

	Entity running = scene.GetEntityByID(id);
	PushMove(history, scene, running, 5.0f);
	DragTo(history, running, 6.0f);
	CHECK_FALSE(history.HasOpenStep());
	CHECK(history.GetStepCount() == 2);
	CHECK_FALSE(history.CanUndo());
	CHECK_FALSE(history.CanRedo());
	history.Undo(scene);
	CHECK(X(running) == 6.0f);

	// What the editor does on Stop
	scene.OnRuntimeStop();
	history.SetEnabled(true);

	Entity stopped = scene.GetEntityByID(id);
	REQUIRE(stopped);
	CHECK(X(stopped) == 2.0f);
	history.Undo(scene);
	CHECK(X(stopped) == 1.0f);
	history.Undo(scene);
	CHECK(X(stopped) == 0.0f);
	history.Redo(scene);
	CHECK(X(stopped) == 1.0f);
}

TEST_CASE("Commands find their entity by UUID after it is deleted and undone")
{
	RegisterComponents();
	Scene scene("");
	Entity parent = scene.CreateEntity("Parent");
	Entity first = CreateMovable(scene, "First");
	Entity middle = CreateMovable(scene, "Middle");
	Entity last = CreateMovable(scene, "Last");
	parent.AddChild(first);
	parent.AddChild(middle);
	parent.AddChild(last);
	const Uuid id = middle.GetID();
	SceneHistory history;

	PushMove(history, scene, middle, 3.0f);

	Scope<DeleteEntitiesCommand> remove = CreateScope<DeleteEntitiesCommand>(std::vector<Entity>{ middle });
	SceneGraph::Remove(middle);
	history.Push(scene, std::move(remove));
	CHECK_FALSE(scene.GetEntityByID(id));

	history.Undo(scene);
	Entity restored = scene.GetEntityByID(id);
	REQUIRE(restored);
	CHECK(restored.GetName() == "Middle");
	CHECK(restored.GetParent() == parent);
	CHECK(first.GetSibling() == restored);
	CHECK(X(restored) == 3.0f);

	history.Undo(scene);
	CHECK(X(scene.GetEntityByID(id)) == 0.0f);
	history.Redo(scene);
	history.Redo(scene);
	CHECK_FALSE(scene.GetEntityByID(id));
}

TEST_CASE("An entity created under a parent comes back under it on redo")
{
	RegisterComponents();
	Scene scene("");
	Entity parent = scene.CreateEntity("Parent");
	SceneHistory history;

	// As the Create menu does: parented first, then recorded
	Entity child = scene.CreateEntity("Child");
	parent.AddChild(child);
	const Uuid id = child.GetID();
	history.Push(scene, CreateScope<CreateEntitiesCommand>(std::vector<Entity>{ child }));

	history.Undo(scene);
	CHECK_FALSE(scene.GetEntityByID(id));
	history.Redo(scene);
	Entity redone = scene.GetEntityByID(id);
	REQUIRE(redone);
	CHECK(redone.GetParent() == parent);
}

TEST_CASE("A deleted child comes back in the middle with its UUIDs and data, and older edits still apply")
{
	RegisterComponents();
	Scene scene("");
	Entity parent = scene.CreateEntity("Parent");
	Entity first = CreateMovable(scene, "First");
	Entity middle = ComponentFixture::Create(scene);
	Entity last = CreateMovable(scene, "Last");
	parent.AddChild(first);
	parent.AddChild(middle);
	parent.AddChild(last);
	const Uuid id = middle.GetID();
	const Uuid childId = middle.GetChild().GetID();
	SceneHistory history;
	EntitySelection selection;
	history.SetSelection(&selection);

	PushMove(history, scene, middle, 8.0f);
	const std::string saved = SceneSerializer::SerializeEntity(middle);

	// As the Hierarchy does: the selection is captured first and cleared before the step is pushed
	selection.Select(id);
	const SelectionState selectedBefore = selection.Capture();
	Scope<DeleteEntitiesCommand> deleteCommand = CreateScope<DeleteEntitiesCommand>(std::vector<Entity>{ middle });
	middle.Destroy();
	selection.Clear();
	history.Push(scene, std::move(deleteCommand), &selectedBefore);
	CHECK(Structure(scene) == "Parent(First Last)");
	CHECK_FALSE(scene.GetEntityByID(id));
	CHECK_FALSE(scene.GetEntityByID(childId));

	history.Undo(scene);
	CHECK(Structure(scene) == "Parent(First Root(Child) Last)");
	Entity restored = scene.GetEntityByID(id);
	REQUIRE(restored);
	CHECK(restored.GetChild().GetID() == childId);
	CHECK(SceneSerializer::SerializeEntity(restored) == saved);
	CHECK(selection.GetAll() == std::vector<Uuid>{ id });

	// The move before the delete undoes on the restored entity
	history.Undo(scene);
	ComponentFixture::Check(scene.GetEntityByID(id));

	history.Redo(scene);
	CHECK(X(scene.GetEntityByID(id)) == 8.0f);
	history.Redo(scene);
	CHECK(Structure(scene) == "Parent(First Last)");
}

TEST_CASE("A root deleted from the middle of the roots comes back in place")
{
	RegisterComponents();
	Scene scene("");
	scene.CreateEntity("A");
	Entity b = scene.CreateEntity("B");
	scene.CreateEntity("C");
	b.AddChild(scene.CreateEntity("D"));
	CHECK(Structure(scene) == "A B(D) C");
	SceneHistory history;

	Scope<DeleteEntitiesCommand> deleteCommand = CreateScope<DeleteEntitiesCommand>(std::vector<Entity>{ b });
	b.Destroy();
	history.Push(scene, std::move(deleteCommand));
	CHECK(Structure(scene) == "A C");

	history.Undo(scene);
	CHECK(Structure(scene) == "A B(D) C");
	history.Redo(scene);
	CHECK(Structure(scene) == "A C");
}

TEST_CASE("Deleting several entities is one step that puts them all back in place")
{
	RegisterComponents();
	Scene scene("");
	Entity parent = scene.CreateEntity("P");
	std::vector<Entity> children;
	for (const char* name : { "A", "B", "C", "D" })
	{
		children.push_back(scene.CreateEntity(name));
		parent.AddChild(children.back());
	}
	Entity root = scene.CreateEntity("R");
	SceneHistory history;

	// Given out of order, as a selection can be
	Scope<DeleteEntitiesCommand> deleteCommand = CreateScope<DeleteEntitiesCommand>(std::vector<Entity>{ children[2], children[1], root });
	for (Entity entity : { children[2], children[1], root })
		entity.Destroy();
	history.Push(scene, std::move(deleteCommand));
	CHECK(Structure(scene) == "P(A D)");
	CHECK(history.GetStepCount() == 1);

	history.Undo(scene);
	CHECK(Structure(scene) == "P(A B C D) R");

	SUBCASE("an entity under another in the list is part of its subtree")
	{
		Scope<DeleteEntitiesCommand> nested = CreateScope<DeleteEntitiesCommand>(std::vector<Entity>{ scene.GetEntityByName("B"), scene.GetEntityByName("P") });
		scene.GetEntityByName("P").Destroy();
		history.Push(scene, std::move(nested));
		CHECK(Structure(scene) == "R");
		history.Undo(scene);
		CHECK(Structure(scene) == "P(A B C D) R");
	}
}

TEST_CASE("Redoing a duplicate gives the copies the same UUIDs as the first time")
{
	RegisterComponents();
	Scene scene("");
	Entity parent = scene.CreateEntity("Parent");
	Entity original = scene.CreateEntity("Original");
	original.AddChild(scene.CreateEntity("Inner"));
	parent.AddChild(original);
	parent.AddChild(scene.CreateEntity("After"));
	SceneHistory history;

	// As the Hierarchy does: new UUIDs when the user acts
	Entity copy = scene.DuplicateEntity(original, Entity());
	copy.GetName() += "_copy";
	const Uuid copyId = copy.GetID();
	const Uuid innerId = copy.GetChild().GetID();
	CHECK(copyId != original.GetID());
	history.Push(scene, CreateScope<CreateEntitiesCommand>(std::vector<Entity>{ copy }));
	CHECK(Structure(scene) == "Parent(Original(Inner) After Original_copy(Inner))");

	history.Undo(scene);
	CHECK_FALSE(scene.GetEntityByID(copyId));
	CHECK(Structure(scene) == "Parent(Original(Inner) After)");

	history.Redo(scene);
	Entity redone = scene.GetEntityByID(copyId);
	REQUIRE(redone);
	CHECK(redone.GetChild().GetID() == innerId);
	CHECK(Structure(scene) == "Parent(Original(Inner) After Original_copy(Inner))");
}

TEST_CASE("Pasting keeps the UUIDs it was given through undo and redo")
{
	RegisterComponents();
	Scene scene("");
	Entity source = scene.CreateEntity("Source");
	const std::string clipboard = SceneSerializer::SerializeEntity(source);
	SceneHistory history;

	Entity pasted = SceneSerializer::DeserializeEntity(&scene, clipboard, true);
	const Uuid id = pasted.GetID();
	CHECK(id != source.GetID());
	history.Push(scene, CreateScope<CreateEntitiesCommand>(std::vector<Entity>{ pasted }));

	history.Undo(scene);
	CHECK(Structure(scene) == "Source");
	history.Redo(scene);
	CHECK(scene.GetEntityByID(id));
	CHECK(Structure(scene) == "Source Source");
}

TEST_CASE("Reparenting and reordering round trip to the same structure")
{
	RegisterComponents();
	Scene scene("");
	Entity a = scene.CreateEntity("A");
	Entity b = scene.CreateEntity("B");
	Entity c = scene.CreateEntity("C");
	Entity d = scene.CreateEntity("D");
	a.AddChild(b);
	a.AddChild(c);
	a.AddChild(d);
	Entity e = scene.CreateEntity("E");
	Entity f = scene.CreateEntity("F");
	const std::string start = "A(B C D) E F";
	REQUIRE(Structure(scene) == start);
	SceneHistory history;

	struct Move
	{
		const char* name;
		std::vector<Entity> entities;
		std::function<void()> move;
		const char* after;
	};
	const std::vector<Move> moves = {
		{ "reorder a child to the front", { d }, [&] { SceneGraph::MoveBefore(d, b); }, "A(D B C) E F" },
		{ "reorder a child to the end", { b }, [&] { SceneGraph::MoveAfter(b, d); }, "A(C D B) E F" },
		{ "reparent a child", { c }, [&] { SceneGraph::Reparent(c, e); }, "A(B D) E(C) F" },
		{ "make a child a root", { c }, [&] { SceneGraph::Unparent(c); }, "A(B D) E F C" },
		{ "reorder a root to the front", { f }, [&] { SceneGraph::MoveBefore(f, a); }, "F A(B C D) E" },
		{ "reorder a root to the end", { a }, [&] { SceneGraph::MoveAfter(a, f); }, "E F A(B C D)" },
		{ "reparent a root", { e }, [&] { SceneGraph::Reparent(e, a); }, "A(B C D E) F" },
		{ "move a root under a child", { f }, [&] { SceneGraph::MoveBefore(f, c); }, "A(B F C D) E" },
		{ "move two siblings together", { c, b }, [&] { SceneGraph::Reparent(b, f); SceneGraph::Reparent(c, f); }, "A(D) E F(B C)" },
	};
	for (const Move& move : moves)
	{
		SUBCASE(move.name)
		{
			PushReparent(history, scene, move.entities, move.move);
			REQUIRE(history.GetStepCount() == 1);
			CHECK(Structure(scene) == move.after);
			history.Undo(scene);
			CHECK(Structure(scene) == start);
			history.Redo(scene);
			CHECK(Structure(scene) == move.after);
			history.Undo(scene);
			CHECK(Structure(scene) == start);
		}
	}

	SUBCASE("a drop that changes nothing pushes no step")
	{
		PushReparent(history, scene, { c }, [&] { SceneGraph::MoveAfter(c, b); });
		CHECK(history.GetStepCount() == 0);
	}

	SUBCASE("several moves undo back in order")
	{
		PushReparent(history, scene, { d }, [&] { SceneGraph::MoveBefore(d, b); });
		PushReparent(history, scene, { b }, [&] { SceneGraph::Reparent(b, f); });
		PushReparent(history, scene, { f }, [&] { SceneGraph::MoveBefore(f, a); });
		CHECK(Structure(scene) == "F(B) A(D C) E");
		while (history.CanUndo())
			history.Undo(scene);
		CHECK(Structure(scene) == start);
		while (history.CanRedo())
			history.Redo(scene);
		CHECK(Structure(scene) == "F(B) A(D C) E");
	}
}

TEST_CASE("A group is one step, undone in reverse")
{
	RegisterComponents();
	Scene scene("");
	Entity entity = CreateMovable(scene, "Entity");
	std::vector<std::string> log;
	SceneHistory history;

	SUBCASE("pushed commands")
	{
		{
			HistoryGroup group(history, scene, "Both");
			history.Push(scene, CreateScope<SizedCommand>("1", 10, log));
			history.Push(scene, CreateScope<SizedCommand>("2", 10, log));
		}
		CHECK(history.GetStepCount() == 1);
		history.Undo(scene);
		history.Redo(scene);
		CHECK(log == std::vector<std::string>{ "undo 2", "undo 1", "redo 1", "redo 2" });
	}

	SUBCASE("an open step inside the group is part of it")
	{
		PushMove(history, scene, entity, 1.0f);
		{
			HistoryGroup group(history, scene, "Move and add");
			DragTo(history, entity, 2.0f);
			Entity created = scene.CreateEntity("Created");
			history.Push(scene, CreateScope<CreateEntitiesCommand>(std::vector<Entity>{ created }));
			DragTo(history, entity, 3.0f);
		}
		CHECK(history.GetStepCount() == 2);
		CHECK(Structure(scene) == "Entity Created");
		history.Undo(scene);
		CHECK(X(entity) == 1.0f);
		CHECK(Structure(scene) == "Entity");
		history.Redo(scene);
		CHECK(X(entity) == 3.0f);
		CHECK(Structure(scene) == "Entity Created");
	}

	SUBCASE("a step open before the group isn't part of it")
	{
		DragTo(history, entity, 1.0f);
		{
			HistoryGroup group(history, scene, "Add");
			history.Push(scene, CreateScope<CreateEntitiesCommand>(std::vector<Entity>{ scene.CreateEntity("Created") }));
		}
		CHECK(history.GetStepCount() == 2);
	}

	SUBCASE("nested groups join the outer one")
	{
		{
			HistoryGroup outer(history, scene, "Outer");
			history.Push(scene, CreateScope<SizedCommand>("1", 10, log));
			{
				HistoryGroup inner(history, scene, "Inner");
				history.Push(scene, CreateScope<SizedCommand>("2", 10, log));
			}
			history.Push(scene, CreateScope<SizedCommand>("3", 10, log));
		}
		CHECK(history.GetStepCount() == 1);
		history.Undo(scene);
		CHECK(log == std::vector<std::string>{ "undo 3", "undo 2", "undo 1" });
	}

	SUBCASE("a group with no change pushes nothing")
	{
		{
			HistoryGroup group(history, scene, "Nothing");
			history.Push(scene, CreateScope<SetPropertyCommand>(std::vector<Entity>{ entity }, PropertyAt("Transform.Position")));
			DragTo(history, entity, 0.0f);
		}
		CHECK(history.GetStepCount() == 0);
		CHECK_FALSE(history.CanUndo());
	}

	SUBCASE("nothing is grouped in Play")
	{
		history.SetEnabled(false);
		{
			HistoryGroup group(history, scene, "Play");
			history.Push(scene, CreateScope<SizedCommand>("1", 10, log));
		}
		history.SetEnabled(true);
		CHECK(history.GetStepCount() == 0);
		CHECK_FALSE(history.IsGrouping());
	}
}

TEST_CASE("An entity created under the selection comes back under it, after its siblings, with its components")
{
	RegisterComponents();
	Scene scene("");
	Entity parent = scene.CreateEntity("Parent");
	parent.AddChild(scene.CreateEntity("Older"));
	scene.CreateEntity("After");
	SceneHistory history;

	// As the Create menu does: components and parent first, then recorded
	Entity created = scene.CreateEntity("Circle");
	created.AddComponent<TransformComponent>();
	created.AddComponent<CircleRendererComponent>();
	parent.AddChild(created);
	const Uuid id = created.GetID();
	history.Push(scene, CreateScope<CreateEntitiesCommand>(std::vector<Entity>{ created }));
	CHECK(Structure(scene) == "Parent(Older Circle) After");

	history.Undo(scene);
	CHECK(Structure(scene) == "Parent(Older) After");
	history.Redo(scene);
	CHECK(Structure(scene) == "Parent(Older Circle) After");
	Entity redone = scene.GetEntityByID(id);
	REQUIRE(redone);
	CHECK(redone.HasComponent<CircleRendererComponent>());
}

TEST_CASE("Instantiating a scene returns its roots, which undo and redo as one step")
{
	RegisterComponents();
	Ref<Scene> prefab = CreateRef<Scene>("");
	Entity first = prefab->CreateEntity("First");
	first.AddChild(prefab->CreateEntity("Inner"));
	prefab->CreateEntity("Second");

	Scene scene("");
	scene.CreateEntity("Existing");
	SceneHistory history;

	std::vector<Entity> roots = scene.InstantiateScene(prefab, Vector3f());
	REQUIRE(roots.size() == 2);
	CHECK(roots[0].GetName() == "First");
	CHECK(roots[1].GetName() == "Second");
	history.Push(scene, CreateScope<CreateEntitiesCommand>(roots));
	CHECK(history.GetStepCount() == 1);
	CHECK(Structure(scene) == "Existing First(Inner) Second");

	history.Undo(scene);
	CHECK(Structure(scene) == "Existing");
	history.Redo(scene);
	CHECK(Structure(scene) == "Existing First(Inner) Second");
}

TEST_CASE("Removing any component and undoing gives its values back, and undoing an add leaves none")
{
	RegisterComponents();
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);
	Entity child = root.GetChild();
	SceneHistory history;

	for (const Reflection::ComponentType* type : Reflection::Components())
	{
		if (type->IsInternal())
			continue;
		for (Entity entity : { root, child })
		{
			if (!type->tryGet(scene.GetRegistry(), entity.GetHandle()))
				continue;
			INFO(type->name);
			const Reflection::ComponentValues fixture = Reflection::Capture(entity, *type);

			Scope<AddRemoveComponentCommand> remove = CreateScope<AddRemoveComponentCommand>(std::vector<Entity>{ entity }, *type,
				AddRemoveComponentCommand::Action::Remove);
			type->remove(scene.GetRegistry(), entity.GetHandle());
			const size_t position = history.GetPosition();
			history.Push(scene, std::move(remove));
			REQUIRE(history.GetPosition() == position + 1);

			history.Undo(scene);
			REQUIRE(type->tryGet(scene.GetRegistry(), entity.GetHandle()));
			CHECK(SameValues(Reflection::Capture(entity, *type), fixture));
			history.Redo(scene);
			CHECK_FALSE(type->tryGet(scene.GetRegistry(), entity.GetHandle()));
			history.Undo(scene);
		}
	}
	ComponentFixture::Check(root);

	Entity bare = scene.CreateEntity("Bare");
	for (const Reflection::ComponentType* type : Reflection::Components())
	{
		if (type->IsInternal())
			continue;
		INFO(type->name);
		Scope<AddRemoveComponentCommand> add = CreateScope<AddRemoveComponentCommand>(std::vector<Entity>{ bare }, *type,
			AddRemoveComponentCommand::Action::Add);
		type->emplace(scene.GetRegistry(), bare.GetHandle());
		const Reflection::ComponentValues added = Reflection::Capture(bare, *type);
		history.Push(scene, std::move(add));

		history.Undo(scene);
		CHECK_FALSE(type->tryGet(scene.GetRegistry(), bare.GetHandle()));
		history.Redo(scene);
		REQUIRE(type->tryGet(scene.GetRegistry(), bare.GetHandle()));
		CHECK(SameValues(Reflection::Capture(bare, *type), added));
		history.Undo(scene);
		CHECK_FALSE(type->tryGet(scene.GetRegistry(), bare.GetHandle()));
	}
}

TEST_CASE("Setting any property to the fixture's value undoes to the default and redoes to the fixture's")
{
	RegisterComponents();
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);
	Entity child = root.GetChild();
	SceneHistory history;
	int tested = 0;

	for (const Reflection::ComponentType* type : Reflection::Components())
	{
		if (type->IsInternal())
			continue;
		for (Entity entity : { root, child })
		{
			if (!type->tryGet(scene.GetRegistry(), entity.GetHandle()))
				continue;
			const Reflection::ComponentValues fixture = Reflection::Capture(entity, *type);
			for (int i = 0; i < (int)type->fields.size(); ++i)
			{
				const Reflection::FieldInfo& field = type->fields[i];
				if (field.Has(Reflection::FieldFlags::ReadOnly) || (!field.set && !field.IsCustom()))
					continue;
				const Reflection::PropertyPath path{ type, i };
				INFO(path.ToString());
				const Reflection::Value fixtureValue = *HistoryCommands::ReadProperty(entity, path);

				// The rest of the component keeps the fixture's values
				Reflection::Restore(entity, fixture, Reflection::ChangeSource::Load);
				HistoryCommands::WriteProperty(entity, path, DefaultOf(*type, i));
				const Reflection::Value start = *HistoryCommands::ReadProperty(entity, path);
				if (start == fixtureValue)
					continue;
				const Reflection::ComponentValues before = Reflection::Capture(entity, *type);

				// As the inspector does
				HistoryCommands::RecordProperty(history, PropertyEdits::NewInteraction(), { entity }, path);
				if (field.IsCustom())
					REQUIRE(HistoryCommands::WriteProperty(entity, path, fixtureValue));
				else
					REQUIRE(Reflection::Set(entity, path, fixtureValue, Reflection::ChangeSource::Editor));
				CHECK(*HistoryCommands::ReadProperty(entity, path) == fixtureValue);
				const Reflection::ComponentValues after = Reflection::Capture(entity, *type);
				const size_t position = history.GetPosition();
				history.End(scene);
				REQUIRE(history.GetPosition() == position + 1);
				++tested;

				history.Undo(scene);
				CHECK(*HistoryCommands::ReadProperty(entity, path) == start);
				CHECK(SameValues(Reflection::Capture(entity, *type), before));
				history.Redo(scene);
				CHECK(*HistoryCommands::ReadProperty(entity, path) == fixtureValue);
				CHECK(SameValues(Reflection::Capture(entity, *type), after));
			}
			Reflection::Restore(entity, fixture, Reflection::ChangeSource::Load);
		}
	}
	ComponentFixture::Check(root);
	CHECK(tested > 100);
}

TEST_CASE("Recording every field of every component with no change pushes nothing")
{
	RegisterComponents();
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);
	SceneHistory history;

	for (const Reflection::ComponentType* type : Reflection::Components())
	{
		if (type->IsInternal() || !type->tryGet(scene.GetRegistry(), root.GetHandle()))
			continue;
		INFO(type->name);
		HistoryCommands::RecordComponent(history, PropertyEdits::NewInteraction(), { root }, *type);
		CHECK(history.HasOpenStep());
		history.End(scene);
		CHECK(history.GetStepCount() == 0);
	}
}

TEST_CASE("One interaction is one step whatever it changes, and a drag back to the start pushes nothing")
{
	RegisterComponents();
	Scene scene("");
	Entity entity = CreateMovable(scene, "Mover");
	const Colour tint = entity.AddComponent<SpriteComponent>().tint;
	scene.MakeClean();
	SceneHistory history;
	PropertyEdits edits(&history);
	const Reflection::PropertyPath position = PropertyAt("Transform.Position");

	SUBCASE("a drag")
	{
		edits.Begin(entity, position);
		for (float x : { 1.0f, 2.0f, 3.0f })
			REQUIRE(edits.Set(entity, position, Vector3f(x, 0.0f, 0.0f)));
		edits.End();
		history.End(scene);
		REQUIRE(history.GetStepCount() == 1);
		history.Undo(scene);
		CHECK(X(entity) == 0.0f);
		history.Redo(scene);
		CHECK(X(entity) == 3.0f);
	}

	SUBCASE("several properties of several components")
	{
		const uint32_t interaction = PropertyEdits::NewInteraction();
		for (float x : { 1.0f, 2.0f })
		{
			HistoryCommands::RecordProperty(history, interaction, { entity }, position);
			HistoryCommands::RecordProperty(history, interaction, { entity }, PropertyAt("Sprite.Tint"));
			SetX(entity, x);
			entity.GetComponent<SpriteComponent>().tint = Colour(x, 0.0f, 0.0f, 1.0f);
		}
		history.End(scene);
		REQUIRE(history.GetStepCount() == 1);
		history.Undo(scene);
		CHECK(X(entity) == 0.0f);
		CHECK(entity.GetComponent<SpriteComponent>().tint == tint);
		history.Redo(scene);
		CHECK(X(entity) == 2.0f);
		CHECK(entity.GetComponent<SpriteComponent>().tint == Colour(2.0f, 0.0f, 0.0f, 1.0f));
	}

	SUBCASE("two drags are two steps")
	{
		for (float x : { 1.0f, 2.0f })
		{
			edits.Begin(entity, position);
			REQUIRE(edits.Set(entity, position, Vector3f(x, 0.0f, 0.0f)));
			edits.End();
		}
		history.End(scene);
		CHECK(history.GetStepCount() == 2);
	}

	SUBCASE("dragged back to the start")
	{
		edits.Begin(entity, position);
		REQUIRE(edits.Set(entity, position, Vector3f(4.0f, 0.0f, 0.0f)));
		REQUIRE(edits.Set(entity, position, Vector3f(0.0f, 0.0f, 0.0f)));
		edits.End();
		history.End(scene);
		CHECK(history.GetStepCount() == 0);
		CHECK_FALSE(scene.IsDirty());
	}
}

TEST_CASE("A resize in the inspector undoes with the tiles it cut off")
{
	RegisterComponents();
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);
	TilemapComponent& tilemap = root.GetComponent<TilemapComponent>();
	const std::vector<std::vector<uint32_t>> tiles = tilemap.tiles;
	SceneHistory history;
	PropertyEdits edits(&history);

	const Reflection::PropertyPath width = PropertyAt("Tilemap.TilesWide");
	edits.Begin(root, width);
	REQUIRE(edits.Set(root, width, 1u));
	edits.End();
	history.End(scene);
	REQUIRE(history.GetStepCount() == 1);
	CHECK(tilemap.tiles == std::vector<std::vector<uint32_t>>{ { 1 }, { 4 } });

	history.Undo(scene);
	CHECK(tilemap.tilesWide == 3);
	CHECK(tilemap.tiles == tiles);
	history.Redo(scene);
	CHECK(tilemap.tiles == std::vector<std::vector<uint32_t>>{ { 1 }, { 4 } });
}

TEST_CASE("A paint stroke stores only the cells it changed, and undo puts them back")
{
	RegisterComponents();
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);
	TilemapComponent& tilemap = root.GetComponent<TilemapComponent>();
	const std::vector<std::vector<uint32_t>> start = tilemap.tiles;
	REQUIRE(start == std::vector<std::vector<uint32_t>>{ { 1, 2, 3 }, { 4, 5, 6 } });
	SceneHistory history;

	Scope<SetTilesCommand> stroke = CreateScope<SetTilesCommand>(root);
	tilemap.tiles[0][1] = 9;
	tilemap.tiles[1][2] = 7;
	tilemap.tiles[1][0] = 4;
	const std::vector<std::vector<uint32_t>> painted = tilemap.tiles;
	REQUIRE(stroke->End(scene));
	const std::vector<SetTilesCommand::Cell>& cells = stroke->GetCells();
	REQUIRE(cells.size() == 2);
	CHECK(cells[0].index == 1);
	CHECK(cells[0].oldTile == 2);
	CHECK(cells[0].newTile == 9);
	CHECK(cells[1].index == 5);
	CHECK(cells[1].oldTile == 6);
	CHECK(cells[1].newTile == 7);
	CHECK(stroke->GetSize() <= sizeof(SetTilesCommand) + 4 * sizeof(SetTilesCommand::Cell));
	history.Push(scene, std::move(stroke));
	REQUIRE(history.GetStepCount() == 1);

	history.Undo(scene);
	CHECK(tilemap.tiles == start);
	history.Redo(scene);
	CHECK(tilemap.tiles == painted);

	SUBCASE("a stroke over several frames, as the Tilemap Editor makes")
	{
		history.Begin(scene, SetTilesCommand::KeyOf(root), CreateScope<SetTilesCommand>(root));
		tilemap.tiles[0][0] = 8;
		history.Begin(scene, SetTilesCommand::KeyOf(root), CreateScope<SetTilesCommand>(root));
		tilemap.tiles[0][2] = 8;
		history.End(scene);
		REQUIRE(history.GetStepCount() == 2);
		history.Undo(scene);
		CHECK(tilemap.tiles == painted);
	}

	SUBCASE("a stroke that changed nothing")
	{
		history.Begin(scene, SetTilesCommand::KeyOf(root), CreateScope<SetTilesCommand>(root));
		history.End(scene);
		CHECK(history.GetStepCount() == 1);
	}
}

TEST_CASE("A gizmo step moving, turning and scaling two entities undoes and redoes exactly")
{
	RegisterComponents();
	Scene scene("");
	Entity a = CreateMovable(scene, "A");
	Entity b = CreateMovable(scene, "B");
	a.GetComponent<TransformComponent>().SetEulerAngles(Vector3f(0.1f, 0.2f, 0.3f));
	b.GetComponent<TransformComponent>().SetRotation(Quaternion(Vector3f(1.0f, 1.0f, 0.0f), 0.4f));
	SceneHistory history;

	struct Pose
	{
		Vector3f position;
		Quaternion rotation;
		Vector3f scale;

		bool operator==(const Pose& other) const
		{
			return position == other.position && rotation == other.rotation && scale == other.scale;
		}
	};
	auto poseOf = [](Entity entity)
		{
			const TransformComponent& transform = entity.GetComponent<TransformComponent>();
			return Pose{ transform.GetPosition(), transform.GetRotation(), transform.GetScale() };
		};
	const Pose startA = poseOf(a);
	const Pose startB = poseOf(b);

	// As the gizmo does each frame of one drag
	const uint32_t interaction = PropertyEdits::NewInteraction();
	const Reflection::ComponentType& type = Reflection::TypeOf<TransformComponent>();
	for (int frame = 1; frame <= 3; ++frame)
	{
		for (Entity entity : { a, b })
		{
			for (const char* field : { "Position", "Orientation", "Scale" })
				HistoryCommands::RecordProperty(history, interaction, { entity }, Reflection::PropertyPath{ &type, type.FindField(field) });
			TransformComponent& transform = entity.GetComponent<TransformComponent>();
			const float offset = entity == a ? 1.0f : 2.0f;
			transform.SetPosition(transform.GetPosition() + Vector3f((float)frame, offset, 0.0f));
			transform.SetRotation(Quaternion(Vector3f(0.0f, 1.0f, 0.0f), 0.1f * offset) * transform.GetRotation());
			transform.SetScale(transform.GetScale() * offset);
		}
	}
	const Pose endA = poseOf(a);
	const Pose endB = poseOf(b);
	history.End(scene);
	REQUIRE(history.GetStepCount() == 1);

	history.Undo(scene);
	CHECK(poseOf(a) == startA);
	CHECK(poseOf(b) == startB);
	history.Redo(scene);
	CHECK(poseOf(a) == endA);
	CHECK(poseOf(b) == endB);
	history.Undo(scene);
	CHECK(poseOf(a) == startA);
	CHECK(poseOf(b) == startB);
}

TEST_CASE("Scene settings undo and redo")
{
	RegisterComponents();
	using Setting = SetSceneSettingCommand::Setting;
	Scene scene("");
	const Vector2f gravity = scene.GetGravity();
	const uint32_t pixels = scene.GetPixelsPerUnit();
	SceneHistory history;

	// A drag of the gravity is one step
	for (const Vector2f& dragged : { Vector2f(0.0f, -5.0f), Vector2f(1.0f, -3.0f) })
	{
		if (!history.IsOpen(SetSceneSettingCommand::KeyOf(Setting::Gravity)))
			history.Begin(scene, SetSceneSettingCommand::KeyOf(Setting::Gravity), CreateScope<SetSceneSettingCommand>(scene, Setting::Gravity));
		scene.SetGravity(dragged);
	}

	Scope<SetSceneSettingCommand> pixelsCommand = CreateScope<SetSceneSettingCommand>(scene, Setting::PixelsPerUnit);
	scene.SetPixelsPerUnit(32);
	history.Push(scene, std::move(pixelsCommand));
	REQUIRE(history.GetStepCount() == 2);

	history.Undo(scene);
	CHECK(scene.GetPixelsPerUnit() == pixels);
	history.Undo(scene);
	CHECK(scene.GetGravity() == gravity);
	history.Redo(scene);
	CHECK(scene.GetGravity() == Vector2f(1.0f, -3.0f));
	history.Redo(scene);
	CHECK(scene.GetPixelsPerUnit() == 32);
}

TEST_CASE("The eye button undoes and redoes, adding the Hidden component in the same step")
{
	RegisterComponents();
	Scene scene("");
	Entity entity = scene.CreateEntity("Lamp");
	SceneHistory history;

	HistoryCommands::SetHidden(history, entity, true);
	REQUIRE(history.GetStepCount() == 1);
	REQUIRE(entity.HasComponent<HiddenComponent>());
	CHECK(entity.GetComponent<HiddenComponent>().hidden);

	HistoryCommands::SetHidden(history, entity, false);
	REQUIRE(history.GetStepCount() == 2);
	CHECK_FALSE(entity.GetComponent<HiddenComponent>().hidden);

	history.Undo(scene);
	CHECK(entity.GetComponent<HiddenComponent>().hidden);
	history.Undo(scene);
	CHECK_FALSE(entity.HasComponent<HiddenComponent>());
	history.Redo(scene);
	REQUIRE(entity.HasComponent<HiddenComponent>());
	CHECK(entity.GetComponent<HiddenComponent>().hidden);
	history.Redo(scene);
	CHECK_FALSE(entity.GetComponent<HiddenComponent>().hidden);
}

TEST_CASE("Undo and redo restore the selection from before and after a step, skipping a missing UUID")
{
	RegisterComponents();
	Scene scene("");
	Entity a = CreateMovable(scene, "A");
	Entity b = CreateMovable(scene, "B");
	Entity c = CreateMovable(scene, "C");
	SceneHistory history;
	EntitySelection selection;
	history.SetSelection(&selection);

	// A step opens with one selection and closes with another
	selection.SetAll({ a.GetID(), b.GetID() }, a.GetID());
	history.Begin(scene, EditKey::ForInteraction(1), CreateScope<SetPropertyCommand>(std::vector<Entity>{ a }, PropertyAt("Transform.Position")));
	SetX(a, 4.0f);
	selection.Select(c.GetID());
	history.End(scene);
	REQUIRE(history.GetStepCount() == 1);

	selection.Select(b.GetID());
	history.Undo(scene);
	CHECK(selection.GetAll() == std::vector<Uuid>{ a.GetID(), b.GetID() });
	CHECK(selection.GetActive() == a.GetID());
	history.Redo(scene);
	CHECK(selection.GetAll() == std::vector<Uuid>{ c.GetID() });

	// A UUID that is gone is skipped
	selection.SetAll({ a.GetID(), b.GetID() });
	const Uuid cId = c.GetID();
	selection.SetAll({ b.GetID(), cId });
	c.Destroy();
	history.Undo(scene);
	CHECK(selection.GetAll() == std::vector<Uuid>{ a.GetID(), b.GetID() });
	selection.Select(a.GetID());
	history.Redo(scene);
	CHECK(selection.IsEmpty());
}

TEST_CASE("Undoing a step whose selection is only a missing entity clears the selection")
{
	RegisterComponents();
	Scene scene("");
	Entity a = CreateMovable(scene, "A");
	SceneHistory history;
	EntitySelection selection;
	history.SetSelection(&selection);

	selection.Select(Uuid(9, 9));
	PushMove(history, scene, a, 1.0f);
	selection.Select(a.GetID());
	history.Undo(scene);
	CHECK(selection.IsEmpty());
	CHECK(selection.GetActive().IsNil());
}

TEST_CASE("Selecting on its own pushes no step")
{
	RegisterComponents();
	Scene scene("");
	Entity a = CreateMovable(scene, "A");
	Entity b = CreateMovable(scene, "B");
	SceneHistory history;
	EntitySelection selection;
	history.SetSelection(&selection);

	scene.MakeClean();
	selection.Select(a.GetID());
	selection.Add(b.GetID());
	selection.Toggle(a.GetID());
	selection.Clear();
	history.End(scene);
	CHECK(history.GetStepCount() == 0);
	CHECK_FALSE(history.CanUndo());
	CHECK_FALSE(scene.IsDirty());

	// The selection a step stores is the one when it opened, however much it changed before
	selection.Select(a.GetID());
	PushMove(history, scene, a, 2.0f);
	selection.Select(b.GetID());
	history.Undo(scene);
	CHECK(selection.GetAll() == std::vector<Uuid>{ a.GetID() });
}

TEST_CASE("A group stores the selection from when it opened and when it closed")
{
	RegisterComponents();
	Scene scene("");
	Entity a = CreateMovable(scene, "A");
	Entity b = CreateMovable(scene, "B");
	SceneHistory history;
	EntitySelection selection;
	history.SetSelection(&selection);

	selection.Select(a.GetID());
	{
		HistoryGroup group(history, scene, "Two moves");
		PushMove(history, scene, a, 1.0f);
		selection.Select(b.GetID());
		PushMove(history, scene, b, 1.0f);
	}
	REQUIRE(history.GetStepCount() == 1);
	history.Undo(scene);
	CHECK(selection.GetAll() == std::vector<Uuid>{ a.GetID() });
	history.Redo(scene);
	CHECK(selection.GetAll() == std::vector<Uuid>{ b.GetID() });
}

TEST_CASE("One gizmo drag of several entities is one step that undoes and redoes each entity's values exactly")
{
	RegisterComponents();
	Scene scene("");
	Entity a = CreateMovable(scene, "A");
	Entity b = CreateMovable(scene, "B");
	b.GetComponent<TransformComponent>().SetPosition(Vector3f(4.0f, 0.0f, 0.0f));
	b.GetComponent<TransformComponent>().SetRotation(Quaternion(Vector3f(0.0f, 0.0f, 1.0f), 0.3f));
	SceneGraph::UpdateWorldTransforms(scene.GetRegistry());
	SceneHistory history;
	EntitySelection selection;
	history.SetSelection(&selection);
	selection.SetAll({ a.GetID(), b.GetID() });

	auto state = [](Entity entity)
	{
		const TransformComponent& transformComp = entity.GetComponent<TransformComponent>();
		return std::make_tuple(transformComp.GetPosition(), transformComp.GetRotation(), transformComp.GetScale());
	};
	const auto beforeA = state(a);
	const auto beforeB = state(b);

	// As the viewport does: record each transform property before the first change, then move every frame of the drag
	const uint32_t interaction = PropertyEdits::NewInteraction();
	const std::vector<Entity> targets = GizmoTransform::GetTargets(scene, selection);
	REQUIRE(targets.size() == 2);
	std::vector<GizmoTransform::Item> items;
	for (Entity target : targets)
		items.push_back(GizmoTransform::ItemOf(target));
	Matrix4x4 pivot = GizmoTransform::PivotMatrix(GizmoTransform::PivotPosition(items, nullptr, GizmoTransform::Pivot::Centre), Quaternion());
	for (int frame = 1; frame <= 3; ++frame)
	{
		const Matrix4x4 next = Matrix4x4::Translate(Vector3f(0.0f, 1.0f, 0.0f)) * pivot
			* Matrix4x4::RotateZ(0.2f) * Matrix4x4::Scale(Vector3f(1.1f, 1.0f, 1.0f));
		for (const char* field : { "Transform.Position", "Transform.Orientation", "Transform.Scale" })
			HistoryCommands::RecordProperty(history, interaction, targets, PropertyAt(field));
		for (Entity target : targets)
		{
			const GizmoTransform::Transform moved = GizmoTransform::Apply(pivot, next, GizmoTransform::ItemOf(target), false);
			TransformComponent& transformComp = target.GetComponent<TransformComponent>();
			transformComp.SetPosition(moved.position);
			transformComp.SetRotation(moved.rotation);
			transformComp.SetScale(moved.scale);
		}
		SceneGraph::UpdateWorldTransforms(scene.GetRegistry());
		pivot = next;
	}
	history.End(scene);
	REQUIRE(history.GetStepCount() == 1);
	const auto afterA = state(a);
	const auto afterB = state(b);
	CHECK(afterA != beforeA);
	CHECK(afterB != beforeB);

	history.Undo(scene);
	CHECK(state(a) == beforeA);
	CHECK(state(b) == beforeB);
	history.Redo(scene);
	CHECK(state(a) == afterA);
	CHECK(state(b) == afterB);
}

TEST_CASE("Setting a value on a mixed selection is one step, undo gives each entity its own value back, and redo sets them all")
{
	using namespace HistoryTests;
	RegisterComponents();
	Scene scene("");
	const std::vector<Entity> entities = { CreateMovable(scene, "A"), CreateMovable(scene, "B"), CreateMovable(scene, "C") };
	for (size_t i = 0; i < entities.size(); ++i)
		SetX(entities[i], (float)i + 1.0f);
	SceneHistory history;
	PropertyEdits edits(&history);
	const Reflection::PropertyPath position = PropertyAt("Transform.Position");
	const Reflection::PropertyPath x = PropertyAt("Transform.Position.X");

	// A drag on the mixed X, starting from the active entity's value
	edits.Begin(entities, position);
	for (float value : { 1.5f, 3.0f, 5.0f })
		REQUIRE(edits.Set(entities, x, value));
	edits.End();
	history.End(scene);
	REQUIRE(history.GetStepCount() == 1);
	for (Entity entity : entities)
		CHECK(X(entity) == 5.0f);

	history.Undo(scene);
	CHECK(X(entities[0]) == 1.0f);
	CHECK(X(entities[1]) == 2.0f);
	CHECK(X(entities[2]) == 3.0f);
	history.Redo(scene);
	for (Entity entity : entities)
		CHECK(X(entity) == 5.0f);

	// A right-click reset is one set of the default, as its own step
	edits.Set(entities, x, 0.0f);
	history.End(scene);
	CHECK(history.GetStepCount() == 2);
	for (Entity entity : entities)
		CHECK(X(entity) == 0.0f);
	history.Undo(scene);
	for (Entity entity : entities)
		CHECK(X(entity) == 5.0f);
}

TEST_CASE("Setting one part of a vector on many entities leaves each one's other parts")
{
	using namespace HistoryTests;
	RegisterComponents();
	Scene scene("");
	Entity a = CreateMovable(scene, "A");
	Entity b = CreateMovable(scene, "B");
	a.GetComponent<TransformComponent>().SetPosition(Vector3f(1.0f, 2.0f, 3.0f));
	b.GetComponent<TransformComponent>().SetPosition(Vector3f(4.0f, 5.0f, 6.0f));
	SceneHistory history;
	PropertyEdits edits(&history);

	const Reflection::PropertyPath y = PropertyAt("Transform.Position.Y");
	edits.Begin({ a, b }, y);
	REQUIRE(edits.Set({ a, b }, y, 9.0f));
	edits.End();
	history.End(scene);

	CHECK(a.GetComponent<TransformComponent>().GetPosition() == Vector3f(1.0f, 9.0f, 3.0f));
	CHECK(b.GetComponent<TransformComponent>().GetPosition() == Vector3f(4.0f, 9.0f, 6.0f));
	REQUIRE(history.GetStepCount() == 1);
	history.Undo(scene);
	CHECK(a.GetComponent<TransformComponent>().GetPosition() == Vector3f(1.0f, 2.0f, 3.0f));
	CHECK(b.GetComponent<TransformComponent>().GetPosition() == Vector3f(4.0f, 5.0f, 6.0f));
	history.Redo(scene);
	CHECK(a.GetComponent<TransformComponent>().GetPosition() == Vector3f(1.0f, 9.0f, 3.0f));
	CHECK(b.GetComponent<TransformComponent>().GetPosition() == Vector3f(4.0f, 9.0f, 6.0f));
}

TEST_CASE("Adding and removing a component on many entities is one step each, undone to each one's values")
{
	using namespace HistoryTests;
	RegisterComponents();
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);
	Entity child = root.GetChild();
	Entity bareA = scene.CreateEntity("Bare A");
	Entity bareB = scene.CreateEntity("Bare B");
	SceneHistory history;

	for (const Reflection::ComponentType* type : Reflection::Components())
	{
		if (type->IsInternal())
			continue;
		INFO(type->name);
		const std::vector<Entity> fixtures = { root, child };
		std::vector<Entity> having;
		std::vector<Reflection::ComponentValues> values;
		for (Entity entity : fixtures)
		{
			if (!type->tryGet(scene.GetRegistry(), entity.GetHandle()))
				continue;
			having.push_back(entity);
			values.push_back(Reflection::Capture(entity, *type));
		}
		if (having.empty())
			continue;

		const size_t position = history.GetPosition();
		MultiEdit::RemoveComponent(&history, fixtures, *type);
		REQUIRE(history.GetPosition() == position + 1);
		for (Entity entity : having)
			CHECK_FALSE(type->tryGet(scene.GetRegistry(), entity.GetHandle()));

		history.Undo(scene);
		for (size_t i = 0; i < having.size(); ++i)
		{
			REQUIRE(type->tryGet(scene.GetRegistry(), having[i].GetHandle()));
			CHECK(SameValues(Reflection::Capture(having[i], *type), values[i]));
		}
		history.Redo(scene);
		for (Entity entity : having)
			CHECK_FALSE(type->tryGet(scene.GetRegistry(), entity.GetHandle()));
		history.Undo(scene);
	}
	ComponentFixture::Check(root);

	for (const Reflection::ComponentType* type : Reflection::Components())
	{
		if (type->IsInternal())
			continue;
		INFO(type->name);
		const bool rootHad = type->tryGet(scene.GetRegistry(), root.GetHandle()) != nullptr;
		const size_t position = history.GetPosition();
		MultiEdit::AddComponent(&history, { bareA, root, bareB }, *type,
			[&](Entity entity) { type->emplace(scene.GetRegistry(), entity.GetHandle()); });
		REQUIRE(history.GetPosition() == position + 1);
		REQUIRE(type->tryGet(scene.GetRegistry(), bareA.GetHandle()));
		REQUIRE(type->tryGet(scene.GetRegistry(), bareB.GetHandle()));
		const Reflection::ComponentValues added = Reflection::Capture(bareA, *type);

		history.Undo(scene);
		CHECK_FALSE(type->tryGet(scene.GetRegistry(), bareA.GetHandle()));
		CHECK_FALSE(type->tryGet(scene.GetRegistry(), bareB.GetHandle()));
		CHECK((type->tryGet(scene.GetRegistry(), root.GetHandle()) != nullptr) == rootHad);
		history.Redo(scene);
		REQUIRE(type->tryGet(scene.GetRegistry(), bareA.GetHandle()));
		REQUIRE(type->tryGet(scene.GetRegistry(), bareB.GetHandle()));
		CHECK(SameValues(Reflection::Capture(bareA, *type), added));
		history.Undo(scene);
	}
	ComponentFixture::Check(root);

	// Nothing to add is no step
	const size_t position = history.GetPosition();
	MultiEdit::AddComponent(&history, { root }, Reflection::TypeOf<TransformComponent>(),
		[](Entity entity) { entity.AddComponent<TransformComponent>(); });
	CHECK(history.GetPosition() == position);
}

TEST_CASE("Renaming many entities is one step that undoes to each one's own name")
{
	using namespace HistoryTests;
	RegisterComponents();
	Scene scene("");
	std::vector<Entity> entities = { scene.CreateEntity("Goblin"), scene.CreateEntity("Orc"), scene.CreateEntity("Troll") };
	SceneHistory history;
	PropertyEdits edits(&history);
	const Reflection::PropertyPath name = PropertyAt("Name.Name");

	// Typed a letter at a time
	edits.Begin(entities, name);
	for (const char* text : { "E", "En", "Enemy" })
		REQUIRE(edits.Set(entities, name, text));
	edits.End();
	history.End(scene);

	REQUIRE(history.GetStepCount() == 1);
	for (Entity entity : entities)
		CHECK(entity.GetName() == "Enemy");
	history.Undo(scene);
	CHECK(entities[0].GetName() == "Goblin");
	CHECK(entities[1].GetName() == "Orc");
	CHECK(entities[2].GetName() == "Troll");
	history.Redo(scene);
	for (Entity entity : entities)
		CHECK(entity.GetName() == "Enemy");
}
