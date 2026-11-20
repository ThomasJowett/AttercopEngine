#include "vendor/doctest.h"

#include "History/HistoryCommands.h"
#include "History/SceneHistory.h"
#include "ComponentFixture.h"
#include "Reflection/Reflection.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneGraph.h"

#include <string>
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

	// Moves the entity as one pushed step
	void PushMove(SceneHistory& history, Scene& scene, Entity entity, float x)
	{
		Scope<EditComponentCommand> command = CreateScope<EditComponentCommand>(entity, Reflection::TypeOf<TransformComponent>());
		SetX(entity, x);
		history.Push(scene, std::move(command));
	}

	// Moves the entity inside an open step
	void DragTo(SceneHistory& history, Scene& scene, Entity entity, float x)
	{
		const EditKey key = ComponentEditKey<TransformComponent>(entity);
		if (!history.IsOpen(key))
			history.Begin(scene, key, CreateScope<EditComponentCommand>(entity, Reflection::TypeOf<TransformComponent>()));
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
	Scene scene("");
	Entity entity = CreateMovable(scene, "Mover");
	SceneHistory history;

	SUBCASE("dragged back to where it started")
	{
		DragTo(history, scene, entity, 4.0f);
		DragTo(history, scene, entity, 0.0f);
		CHECK(history.HasOpenStep());
		history.End(scene);
		CHECK_FALSE(history.HasOpenStep());
		CHECK(history.GetStepCount() == 0);
		CHECK_FALSE(scene.IsDirty());
	}

	SUBCASE("pushed with no change")
	{
		history.Push(scene, CreateScope<EditComponentCommand>(entity, Reflection::TypeOf<TransformComponent>()));
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
	Scene scene("");
	Entity a = CreateMovable(scene, "A");
	Entity b = CreateMovable(scene, "B");
	SceneHistory history;

	// A drag over many frames is one step, from where it started
	DragTo(history, scene, a, 1.0f);
	DragTo(history, scene, a, 2.0f);
	DragTo(history, scene, a, 3.0f);
	CHECK(history.IsOpen(ComponentEditKey<TransformComponent>(a)));
	CHECK(history.GetStepCount() == 0);

	// A component of the same entity is another key
	CHECK_FALSE(history.IsOpen(ComponentEditKey<NameComponent>(a)));

	DragTo(history, scene, b, 7.0f);
	CHECK(history.GetStepCount() == 1);
	CHECK_FALSE(history.IsOpen(ComponentEditKey<TransformComponent>(a)));
	CHECK(history.IsOpen(ComponentEditKey<TransformComponent>(b)));

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

TEST_CASE("Keys match on entities, component and property")
{
	const Uuid first, second;
	CHECK(EditKey({ first, second }, nullptr) == EditKey({ second, first }, nullptr));
	CHECK(EditKey({ first }, nullptr, "Position") != EditKey({ first }, nullptr, "Scale"));
	CHECK(EditKey({ first }, nullptr) != EditKey({ second }, nullptr));
}

TEST_CASE("The scene is clean at the saved position")
{
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
		DragTo(history, scene, entity, 4.0f);
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
	Scene scene("");
	Entity entity = CreateMovable(scene, "Mover");
	const Uuid id = entity.GetID();
	SceneHistory history;

	PushMove(history, scene, entity, 1.0f);
	DragTo(history, scene, entity, 2.0f);

	// What the editor does on Play
	history.End(scene);
	history.SetEnabled(false);
	scene.OnRuntimeStart();
	CHECK(history.GetStepCount() == 2);

	Entity running = scene.GetEntityByID(id);
	PushMove(history, scene, running, 5.0f);
	DragTo(history, scene, running, 6.0f);
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

	Scope<RemoveEntityCommand> remove = CreateScope<RemoveEntityCommand>(middle);
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
	Scene scene("");
	Entity parent = scene.CreateEntity("Parent");
	SceneHistory history;

	// As the Create menu does: parented first, then recorded
	Entity child = scene.CreateEntity("Child");
	parent.AddChild(child);
	const Uuid id = child.GetID();
	history.Push(scene, CreateScope<AddEntityCommand>(child));

	history.Undo(scene);
	CHECK_FALSE(scene.GetEntityByID(id));
	history.Redo(scene);
	Entity redone = scene.GetEntityByID(id);
	REQUIRE(redone);
	CHECK(redone.GetParent() == parent);
}

TEST_CASE("Adding and removing a component undo and redo")
{
	Scene scene("");
	Entity entity = scene.CreateEntity("Entity");
	SceneHistory history;

	Scope<AddComponentCommand> add = CreateScope<AddComponentCommand>(entity, Reflection::TypeOf<CircleRendererComponent>());
	entity.AddComponent<CircleRendererComponent>().thickness = 0.5f;
	history.Push(scene, std::move(add));

	Scope<RemoveComponentCommand> remove = CreateScope<RemoveComponentCommand>(entity, Reflection::TypeOf<CircleRendererComponent>());
	entity.RemoveComponent<CircleRendererComponent>();
	history.Push(scene, std::move(remove));

	history.Undo(scene);
	REQUIRE(entity.HasComponent<CircleRendererComponent>());
	CHECK(entity.GetComponent<CircleRendererComponent>().thickness == 0.5f);
	history.Undo(scene);
	CHECK_FALSE(entity.HasComponent<CircleRendererComponent>());
	history.Redo(scene);
	REQUIRE(entity.HasComponent<CircleRendererComponent>());
	CHECK(entity.GetComponent<CircleRendererComponent>().thickness == 0.5f);
}

TEST_CASE("The edit command undoes and redoes the whole of every component in the fixture")
{
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
			const Reflection::ComponentValues edited = Reflection::Capture(entity, *type);

			// The edit puts the component back to its defaults
			Scope<EditComponentCommand> command = CreateScope<EditComponentCommand>(entity, *type);
			Reflection::Restore(entity, Reflection::Defaults(*type), Reflection::ChangeSource::Editor);
			const Reflection::ComponentValues defaults = Reflection::Capture(entity, *type);
			const size_t position = history.GetPosition();
			history.Push(scene, std::move(command));
			if (defaults.values == edited.values && defaults.custom == edited.custom)
			{
				CHECK(history.GetPosition() == position);
				continue;
			}
			REQUIRE(history.GetPosition() == position + 1);

			history.Undo(scene);
			Reflection::ComponentValues undone = Reflection::Capture(entity, *type);
			CHECK(undone.values == edited.values);
			CHECK(undone.custom == edited.custom);

			history.Redo(scene);
			Reflection::ComponentValues redone = Reflection::Capture(entity, *type);
			CHECK(redone.values == defaults.values);
			CHECK(redone.custom == defaults.custom);

			// Back to the fixture's values for the next component
			history.Undo(scene);
		}
	}
	ComponentFixture::Check(root);
}

TEST_CASE("Ending an edit that changed nothing pushes no step")
{
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);
	SceneHistory history;

	for (const Reflection::ComponentType* type : Reflection::Components())
	{
		if (type->IsInternal() || !type->tryGet(scene.GetRegistry(), root.GetHandle()))
			continue;
		INFO(type->name);
		history.Begin(scene, ComponentEditKey(root, *type), CreateScope<EditComponentCommand>(root, *type));
		history.End(scene);
		CHECK(history.GetStepCount() == 0);
	}
}
