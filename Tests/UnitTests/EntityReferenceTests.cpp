#include "vendor/doctest.h"
#include "TestHelpers.h"

#include "Core/UUID.h"
#include "History/SceneHistory.h"
#include "Physics/PhysicsEngine2D.h"
#include "Inspector/InspectorWidgets.h"
#include "Reflection/Reflection.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/EntityReference.h"
#include "Scene/Scene.h"
#include "Scene/SceneGraph.h"
#include "Scene/SceneSerializer.h"
#include "Scripting/Lua/LuaManager.h"
#include "Selection/EntitySelection.h"
#include "Selection/SelectionOperations.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace EntityReferenceTests
{
	bool Contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

	// Loads scene XML as if read from a file with this name
	void LoadScene(Scene& scene, const std::string& xml)
	{
		scene.Load(std::vector<uint8_t>(xml.begin(), xml.end()));
	}

	// Every entity with an ID is found through the map, and nothing else is
	void CheckMap(Scene& scene)
	{
		size_t count = 0;
		for (entt::entity handle : scene.GetRegistry().view<IDComponent>())
		{
			Entity entity(handle, &scene);
			CHECK(scene.GetEntityByID(entity.GetID()) == entity);
			++count;
		}
		CHECK(count == scene.GetRegistry().view<IDComponent>().size());
	}

	// A rigid body with a weld joint, unset, under parent if there is one
	Entity Welded(Scene& scene, const std::string& name, Entity parent = Entity())
	{
		Entity entity = scene.CreateEntity(name);
		entity.AddComponent<TransformComponent>();
		entity.AddComponent<RigidBody2DComponent>();
		entity.AddComponent<WeldJoint2DComponent>();
		if (parent)
			parent.AddChild(entity);
		return entity;
	}

	void WeldTo(Entity entity, Entity target)
	{
		entity.GetComponent<WeldJoint2DComponent>().connectedEntity = EntityReference{ target.GetID() };
	}

	const EntityReference& ConnectedEntity(Entity entity)
	{
		return entity.GetComponent<WeldJoint2DComponent>().connectedEntity;
	}

	Entity WeldTarget(Entity entity)
	{
		return ConnectedEntity(entity).Resolve(entity.GetScene());
	}

	// Runs a chunk of Lua; the error text, empty if it ran
	std::string RunLua(const std::string& code)
	{
		sol::protected_function_result result = LuaManager::GetState().safe_script(code, sol::script_pass_on_error);
		if (result.valid())
			return {};
		sol::error error = result;
		return error.what();
	}
}

using namespace EntityReferenceTests;

TEST_CASE("Uuid::Parse reads low-high in decimal")
{
	CHECK(Uuid::Parse("2782515162868888147-6086489932850137559") == Uuid(2782515162868888147ull, 6086489932850137559ull));
	CHECK(Uuid::Parse("18446744073709551615-1") == Uuid(18446744073709551615ull, 1));
	CHECK(Uuid::Parse("1-2") == Uuid(1, 2));

	Uuid id;
	CHECK(Uuid::Parse(id.to_string()) == id);
}

TEST_CASE("Uuid::Parse rejects anything else")
{
	for (const char* text : { "", "-", "1-", "-2", "12345", "1--2", "1-2-3", "-1-2", "+1-2", " 1-2", "1-2 ",
		"12a-34", "abc-def", "0x1-2", "1.5-2" })
	{
		INFO(text);
		CHECK_FALSE(Uuid::Parse(text));
	}
}

TEST_CASE("Uuid::Parse rejects numbers too big for 64 bits")
{
	CHECK_FALSE(Uuid::Parse("18446744073709551616-1"));
	CHECK_FALSE(Uuid::Parse("1-99999999999999999999"));
}

TEST_CASE("The nil UUID is 0-0 and never made for a new entity")
{
	CHECK(Uuid::Nil().IsNil());
	CHECK(Uuid::Nil().to_string() == "0-0");
	CHECK(Uuid::Parse("0-0") == Uuid::Nil());
	CHECK_FALSE(Uuid().IsNil());

	Scene scene("");
	Entity entity = scene.CreateEntity(Uuid::Nil(), "E");
	CHECK_FALSE(entity.GetID().IsNil());
	CHECK(scene.GetEntityByID(entity.GetID()) == entity);
	CHECK_FALSE(scene.GetEntityByID(Uuid::Nil()));
}

TEST_CASE("New UUIDs don't repeat")
{
	std::unordered_set<Uuid> ids;
	for (int i = 0; i < 10000; ++i)
		CHECK(ids.insert(Uuid()).second);
}

TEST_CASE("Created entities are found by ID")
{
	Scene scene("");
	Entity a = scene.CreateEntity("A");
	Entity b = scene.CreateEntity("B");
	Entity child = scene.CreateEntity(Uuid(7, 8), "Child");
	SceneGraph::Reparent(child, a);

	CHECK(scene.GetEntityByID(a.GetID()) == a);
	CHECK(scene.GetEntityByID(b.GetID()) == b);
	CHECK(scene.GetEntityByID(Uuid(7, 8)) == child);
	CHECK_FALSE(scene.GetEntityByID(Uuid(8, 7)));
	CheckMap(scene);
}

TEST_CASE("A destroyed entity isn't found by ID")
{
	Scene scene("");
	Entity parent = scene.CreateEntity("Parent");
	Entity child = scene.CreateEntity("Child");
	Entity other = scene.CreateEntity("Other");
	SceneGraph::Reparent(child, parent);
	const Uuid parentID = parent.GetID(), childID = child.GetID();

	scene.RemoveEntity(parent);

	CHECK_FALSE(scene.GetEntityByID(parentID));
	CHECK_FALSE(scene.GetEntityByID(childID));
	CHECK(scene.GetEntityByID(other.GetID()) == other);
	CheckMap(scene);
}

TEST_CASE("Clearing the registry empties the map")
{
	Scene scene("");
	const Uuid id = scene.CreateEntity("A").GetID();

	scene.GetRegistry().clear();
	SceneGraph::RelinkRoots(scene);
	CHECK_FALSE(scene.GetEntityByID(id));

	Entity again = scene.CreateEntity(id, "A again");
	CHECK(scene.GetEntityByID(id) == again);
}

TEST_CASE("Entities loaded from XML are found by ID")
{
	Scene scene("Loaded.scene");
	LoadScene(scene, R"(<Scene FormatVersion="2">
<Entity Name="Parent" ID="1-2">
<Entity Name="Child" ID="3-4"/>
</Entity>
<Entity Name="Other" ID="5-6"/>
</Scene>)");

	CHECK(scene.GetEntityByID(Uuid(1, 2)).GetName() == "Parent");
	CHECK(scene.GetEntityByID(Uuid(3, 4)).GetName() == "Child");
	CHECK(scene.GetEntityByID(Uuid(5, 6)).GetName() == "Other");
	CheckMap(scene);
}

TEST_CASE("Stopping play keeps the map and entity handles")
{
	Scene scene("");
	Entity kept = scene.CreateEntity("Kept");
	Entity destroyed = scene.CreateEntity("Destroyed during play");
	const Uuid keptID = kept.GetID(), destroyedID = destroyed.GetID();

	scene.OnRuntimeStart();
	const Uuid spawnedID = scene.CreateEntity("Spawned during play").GetID();
	scene.RemoveEntity(destroyed);
	CHECK_FALSE(scene.GetEntityByID(destroyedID));
	CHECK(scene.GetEntityByID(spawnedID));
	scene.OnRuntimeStop();

	CHECK(scene.GetEntityByID(keptID) == kept);
	CHECK(kept.GetName() == "Kept");
	CHECK(scene.GetEntityByID(destroyedID) == destroyed);
	CHECK(destroyed.GetName() == "Destroyed during play");
	CHECK_FALSE(scene.GetEntityByID(spawnedID));
	CheckMap(scene);
}

TEST_CASE("An entity destroyed and loaded back from its XML keeps its UUID")
{
	Scene scene("");
	Entity entity = scene.CreateEntity("Deleted");
	Entity child = scene.CreateEntity("Its child");
	SceneGraph::Reparent(child, entity);
	const Uuid id = entity.GetID(), childID = child.GetID();
	const std::string xml = SceneSerializer::SerializeEntity(entity);

	scene.RemoveEntity(entity);
	REQUIRE_FALSE(scene.GetEntityByID(id));

	WarningCapture warnings;
	Entity restored = SceneSerializer::DeserializeEntity(&scene, xml);
	CHECK(restored.GetID() == id);
	CHECK(scene.GetEntityByID(id) == restored);
	CHECK(scene.GetEntityByID(childID).GetName() == "Its child");
	CHECK(warnings.Text().empty());
}

TEST_CASE("Of two entities with the same UUID in a file, the second gets a new one with a warning")
{
	WarningCapture warnings;
	Scene scene("Duplicates.scene");
	LoadScene(scene, R"(<Scene FormatVersion="2">
<Entity Name="First" ID="5-6">
<Entity Name="Child" ID="5-6"/>
</Entity>
<Entity Name="Second" ID="5-6"/>
</Scene>)");

	CHECK(scene.GetEntityByID(Uuid(5, 6)).GetName() == "First");
	for (const char* name : { "Child", "Second" })
	{
		INFO(name);
		Entity entity = scene.GetEntityByPath(name == std::string("Child") ? "First/Child" : name);
		REQUIRE(entity);
		CHECK(entity.GetID() != Uuid(5, 6));
		CHECK_FALSE(entity.GetID().IsNil());
	}
	CheckMap(scene);

	const std::string text = warnings.Text();
	CHECK(Contains(text, "Child has the same ID as First on line 2, given a new one (Duplicates.scene, line 3)"));
	CHECK(Contains(text, "Second has the same ID as First on line 2, given a new one (Duplicates.scene, line 5)"));
}

TEST_CASE("An entity loaded with the UUID of one already in the scene gets a new one with a warning")
{
	Scene scene("");
	Entity existing = scene.CreateEntity(Uuid(5, 6), "Existing");

	WarningCapture warnings;
	Entity pasted = SceneSerializer::DeserializeEntity(&scene, R"(<Entity Name="Pasted" ID="5-6"/>)");

	CHECK(scene.GetEntityByID(Uuid(5, 6)) == existing);
	CHECK(pasted.GetID() != Uuid(5, 6));
	CHECK(scene.GetEntityByID(pasted.GetID()) == pasted);
	CHECK(Contains(warnings.Text(), "Pasted has the same ID as Existing in the scene, given a new one (the pasted entity, line 1)"));
}

TEST_CASE("An entity created with a UUID already in use gets a new one")
{
	Scene scene("");
	Entity first = scene.CreateEntity(Uuid(5, 6), "First");

	WarningCapture warnings;
	Entity second = scene.CreateEntity(Uuid(5, 6), "Second");

	CHECK(scene.GetEntityByID(Uuid(5, 6)) == first);
	CHECK(second.GetID() != Uuid(5, 6));
	CHECK(scene.GetEntityByID(second.GetID()) == second);
	CHECK(Contains(warnings.Text(), "5-6"));
}

TEST_CASE("A malformed or nil ID in a file is replaced with a warning")
{
	WarningCapture warnings;
	Scene scene("Malformed.scene");
	LoadScene(scene, R"(<Scene FormatVersion="2">
<Entity Name="NoDash" ID="12345"/>
<Entity Name="Nil" ID="0-0"/>
</Scene>)");

	for (const char* name : { "NoDash", "Nil" })
	{
		INFO(name);
		Entity entity = scene.GetEntityByName(name);
		REQUIRE(entity);
		CHECK_FALSE(entity.GetID().IsNil());
	}
	CheckMap(scene);

	const std::string text = warnings.Text();
	CHECK(Contains(text, "NoDash has an invalid ID \"12345\", given a new one (Malformed.scene, line 2)"));
	CHECK(Contains(text, "Nil has an invalid ID \"0-0\", given a new one (Malformed.scene, line 3)"));
}

TEST_CASE("A reference is saved as its target's ID and name, and loads back")
{
	Scene scene("");
	Entity floor = Welded(scene, "Floor");
	Entity crate = Welded(scene, "Crate");
	WeldTo(crate, floor);

	const std::string xml = SceneSerializer::SerializeEntities({ floor, crate });
	CHECK(Contains(xml, "<ConnectedEntity ID=\"" + floor.GetID().to_string() + "\" Name=\"Floor\"/>"));

	Scene loaded("");
	std::vector<Entity> roots = SceneSerializer::DeserializeEntities(&loaded, xml);
	REQUIRE(roots.size() == 2);
	CHECK(roots[0].GetID() == floor.GetID());
	CHECK(WeldTarget(roots[1]) == roots[0]);
}

TEST_CASE("A wrong Name is ignored when loading, and saving writes the target's")
{
	WarningCapture warnings;
	Scene scene("Hand.scene");
	LoadScene(scene, R"(<Scene FormatVersion="2"><Gravity X="0" Y="-9.81"/>
<Entity Name="Floor" ID="1-1"><Transform/><RigidBody2D/></Entity>
<Entity Name="Crate" ID="2-2"><Transform/><RigidBody2D/><WeldJoint2D><ConnectedEntity ID="1-1" Name="Wall"/></WeldJoint2D></Entity>
</Scene>)");

	Entity crate = scene.GetEntityByID(Uuid(2, 2));
	REQUIRE(crate);
	CHECK(WeldTarget(crate).GetName() == "Floor");
	CHECK(warnings.Text().empty());
	CHECK(Contains(SceneSerializer::SerializeEntity(crate), "<ConnectedEntity ID=\"1-1\" Name=\"Floor\"/>"));
}

TEST_CASE("An unset reference is left out of the file and loads unset")
{
	Scene scene("");
	Entity crate = Welded(scene, "Crate");

	const std::string xml = SceneSerializer::SerializeEntity(crate);
	CHECK_FALSE(Contains(xml, "ConnectedEntity"));

	Scene loaded("");
	CHECK_FALSE(ConnectedEntity(SceneSerializer::DeserializeEntity(&loaded, xml)).IsSet());
}

TEST_CASE("A missing target resolves to nothing and is saved unchanged")
{
	Scene scene("Missing.scene");
	LoadScene(scene, R"(<Scene FormatVersion="2">
<Entity Name="Crate" ID="2-2"><Transform/><RigidBody2D/><WeldJoint2D><ConnectedEntity ID="9-9" Name="Gone"/></WeldJoint2D></Entity>
</Scene>)");

	Entity crate = scene.GetEntityByID(Uuid(2, 2));
	REQUIRE(crate);
	CHECK(ConnectedEntity(crate).id == Uuid(9, 9));
	CHECK_FALSE(WeldTarget(crate));
	CHECK(Contains(SceneSerializer::SerializeEntity(crate), "<ConnectedEntity ID=\"9-9\"/>"));
}

TEST_CASE("A reference whose ID can't be read loads unset with a warning naming the file and line")
{
	WarningCapture warnings;
	Scene scene("Bad.scene");
	LoadScene(scene, R"(<Scene FormatVersion="2">
<Entity Name="Crate" ID="2-2"><Transform/><RigidBody2D/><WeldJoint2D>
<ConnectedEntity ID="floor" Colour="red"/>
</WeldJoint2D></Entity>
</Scene>)");

	Entity crate = scene.GetEntityByID(Uuid(2, 2));
	REQUIRE(crate);
	CHECK_FALSE(ConnectedEntity(crate).IsSet());
	const std::string text = warnings.Text();
	CHECK(Contains(text, "Can't read ID=\"floor\" as an entity's ID, left unset (Bad.scene, line 3)"));
	CHECK(Contains(text, "Unknown attribute Colour on <ConnectedEntity>, ignored (Bad.scene, line 3)"));
}

TEST_CASE("Duplicating a parent and child that point at each other points the copies at each other, keeping a reference outside")
{
	Scene scene("");
	Entity outside = Welded(scene, "Outside");
	Entity parent = Welded(scene, "Parent");
	Entity child = Welded(scene, "Child", parent);
	Entity grandchild = Welded(scene, "Grandchild", child);
	WeldTo(parent, child);
	WeldTo(child, parent);
	WeldTo(grandchild, outside);

	Entity copy = scene.DuplicateEntity(parent, Entity());
	REQUIRE(copy);
	Entity copyChild = copy.GetChild();
	REQUIRE(copyChild);
	Entity copyGrandchild = copyChild.GetChild();
	REQUIRE(copyGrandchild);

	CHECK(WeldTarget(copy) == copyChild);
	CHECK(WeldTarget(copyChild) == copy);
	CHECK(WeldTarget(copyGrandchild) == outside);
	CHECK(WeldTarget(parent) == child);
	CHECK(WeldTarget(child) == parent);
}

TEST_CASE("Duplicating two selected roots that point at each other points the copies at each other, through undo and redo")
{
	Scene scene("");
	Entity a = Welded(scene, "A");
	Entity b = Welded(scene, "B");
	WeldTo(a, b);
	WeldTo(b, a);
	SceneHistory history;
	EntitySelection selection;
	selection.SetAll({ a.GetID(), b.GetID() });

	std::vector<Entity> copies = SelectionOperations::Duplicate(history, selection, scene);
	REQUIRE(copies.size() == 2);
	CHECK(copies[0].GetName() == "A_copy");
	CHECK(WeldTarget(copies[0]) == copies[1]);
	CHECK(WeldTarget(copies[1]) == copies[0]);
	CHECK(WeldTarget(a) == b);

	const Uuid copyA = copies[0].GetID(), copyB = copies[1].GetID();
	history.Undo(scene);
	CHECK_FALSE(scene.GetEntityByID(copyA));
	history.Redo(scene);
	REQUIRE(scene.GetEntityByID(copyA));
	CHECK(WeldTarget(scene.GetEntityByID(copyA)).GetID() == copyB);
	CHECK(WeldTarget(scene.GetEntityByID(copyB)).GetID() == copyA);
}

TEST_CASE("Instantiating a two-root prefab points the copies of its roots at each other")
{
	Ref<Scene> prefab = CreateRef<Scene>("");
	Entity a = Welded(*prefab, "A");
	Entity b = Welded(*prefab, "B");
	WeldTo(a, b);
	WeldTo(b, a);

	Scene scene("");
	std::vector<Entity> roots = scene.InstantiateScene(prefab, Vector3f(1.0f, 0.0f, 0.0f));
	REQUIRE(roots.size() == 2);
	CHECK(roots[0].GetName() == "A");
	CHECK(roots[0].GetID() != a.GetID());
	CHECK(WeldTarget(roots[0]) == roots[1]);
	CHECK(WeldTarget(roots[1]) == roots[0]);
}

TEST_CASE("Pasting several entities that point at each other points the pasted ones at each other, keeping a reference outside")
{
	Scene scene("");
	Entity outside = Welded(scene, "Outside");
	Entity a = Welded(scene, "A");
	Entity b = Welded(scene, "B");
	Entity c = Welded(scene, "C");
	WeldTo(a, b);
	WeldTo(b, a);
	WeldTo(c, outside);
	SceneHistory history;
	EntitySelection selection;
	selection.SetAll({ a.GetID(), b.GetID(), c.GetID() });
	const std::string clipboard = SelectionOperations::Copy(selection, scene);

	std::vector<Entity> pasted = SelectionOperations::Paste(history, selection, scene, clipboard);
	REQUIRE(pasted.size() == 3);
	CHECK(pasted[0].GetID() != a.GetID());
	CHECK(WeldTarget(pasted[0]) == pasted[1]);
	CHECK(WeldTarget(pasted[1]) == pasted[0]);
	CHECK(WeldTarget(pasted[2]) == outside);

	// In another scene the outside target is missing, still pointed at
	Scene other("");
	std::vector<Entity> elsewhere = SelectionOperations::Paste(history, selection, other, clipboard);
	REQUIRE(elsewhere.size() == 3);
	CHECK(WeldTarget(elsewhere[0]) == elsewhere[1]);
	CHECK(ConnectedEntity(elsewhere[2]).id == outside.GetID());
	CHECK_FALSE(WeldTarget(elsewhere[2]));
}

TEST_CASE("A reference resolves again once its target is destroyed and loaded back from its XML")
{
	Scene scene("");
	Entity target = Welded(scene, "Target");
	Entity crate = Welded(scene, "Crate");
	WeldTo(crate, target);
	const Uuid id = target.GetID();
	const std::string xml = SceneSerializer::SerializeEntity(target);

	scene.RemoveEntity(target);
	CHECK_FALSE(WeldTarget(crate));
	CHECK(ConnectedEntity(crate).id == id);
	CHECK(Contains(SceneSerializer::SerializeEntity(crate), "<ConnectedEntity ID=\"" + id.to_string() + "\"/>"));

	SceneSerializer::DeserializeEntity(&scene, xml);
	REQUIRE(WeldTarget(crate));
	CHECK(WeldTarget(crate).GetName() == "Target");
}

TEST_CASE("Lua reads a reference as an Entity or nil, and sets it from an Entity in the scene or nil")
{
	LuaManager::Init();
	Scene scene("");
	Entity floor = Welded(scene, "Floor");
	Entity crate = Welded(scene, "Crate");
	Entity gone = Welded(scene, "Gone");
	Scene other("");
	Entity stranger = Welded(other, "Stranger");
	LuaManager::GetState()["floor"] = floor;
	LuaManager::GetState()["crate"] = crate;
	LuaManager::GetState()["gone"] = gone;
	LuaManager::GetState()["stranger"] = stranger;

	CHECK(RunLua(R"(
		local weld = crate:GetWeldJoint2DComponent()
		assert(weld.ConnectedEntity == nil, "unset reads nil")
		weld.ConnectedEntity = floor
		assert(weld.ConnectedEntity:GetName() == "Floor", "reads the target")
	)") == "");
	CHECK(WeldTarget(crate) == floor);

	scene.RemoveEntity(floor);
	CHECK(RunLua("assert(crate:GetWeldJoint2DComponent().ConnectedEntity == nil, 'a missing target reads nil')") == "");

	scene.RemoveEntity(gone);
	CHECK(Contains(RunLua("crate:GetWeldJoint2DComponent().ConnectedEntity = gone"), "Can't point at a destroyed entity"));
	CHECK(Contains(RunLua("crate:GetWeldJoint2DComponent().ConnectedEntity = stranger"), "Can't point at Stranger, an entity in another scene"));
	CHECK(Contains(RunLua("crate:GetWeldJoint2DComponent().ConnectedEntity = 5"), "can't be set to a number; it's an Entity"));

	CHECK(RunLua("crate:GetWeldJoint2DComponent().ConnectedEntity = nil") == "");
	CHECK_FALSE(ConnectedEntity(crate).IsSet());
	LuaManager::Shutdown();
}

TEST_CASE("The inspector's picker lists entities by hierarchy path, filtered by what the field requires and the search")
{
	Scene scene("");
	Entity level = scene.CreateEntity("Level");
	Entity doors = scene.CreateEntity("Doors");
	level.AddChild(doors);
	Entity door = Welded(scene, "Door 2", doors);
	Entity tiles = scene.CreateEntity("Tiles");
	tiles.AddComponent<TilemapComponent>();
	Welded(scene, "Crate");

	const Reflection::ComponentType& weld = Reflection::TypeOf<WeldJoint2DComponent>();
	const Reflection::FieldInfo& field = weld.fields[weld.FindField("ConnectedEntity")];
	CHECK(Inspector::HierarchyPath(door) == "Level/Doors/Door 2");
	CHECK(Reflection::MeetsRequirements(field, door));
	CHECK(Reflection::MeetsRequirements(field, tiles));
	CHECK_FALSE(Reflection::MeetsRequirements(field, level));

	auto paths = [&](std::string_view search)
		{
			std::string text;
			for (const Inspector::EntityChoice& choice : Inspector::EntityChoices(scene, field, search))
				text += (text.empty() ? "" : ", ") + choice.path;
			return text;
		};
	CHECK(paths("") == "Level/Doors/Door 2, Tiles, Crate");
	CHECK(paths("door") == "Level/Doors/Door 2");
	CHECK(paths("LEVEL") == "Level/Doors/Door 2");
	CHECK(paths("nothing") == "");
}

TEST_CASE("When Play starts a weld joins its target's body, and a missing target, itself or a target without a body warns and makes no joint")
{
	Scene scene("");
	// Before its target, so the target's body is made after it
	Entity crate = Welded(scene, "Crate");
	Entity floor = scene.CreateEntity("Floor");
	floor.AddComponent<TransformComponent>();
	floor.AddComponent<RigidBody2DComponent>();
	WeldTo(crate, floor);
	Entity self = Welded(scene, "Self");
	WeldTo(self, self);
	Entity lost = Welded(scene, "Lost");
	lost.GetComponent<WeldJoint2DComponent>().connectedEntity = EntityReference{ Uuid(9, 9) };
	Entity marker = scene.CreateEntity("Marker");
	marker.AddComponent<TransformComponent>();
	Entity plain = Welded(scene, "Plain");
	WeldTo(plain, marker);
	Entity world = Welded(scene, "World");

	WarningCapture warnings;
	scene.OnRuntimeStart();
	const WeldJoint2DComponent& crateWeld = crate.GetComponent<WeldJoint2DComponent>();
	CHECK(crateWeld.joint != nullptr);
	CHECK(crateWeld.bodyB == floor.GetComponent<RigidBody2DComponent>().runtimeBody);
	CHECK(self.GetComponent<WeldJoint2DComponent>().joint == nullptr);
	CHECK(lost.GetComponent<WeldJoint2DComponent>().joint == nullptr);
	CHECK(plain.GetComponent<WeldJoint2DComponent>().joint == nullptr);
	// Unset, with no parent, still welds to the world
	CHECK(world.GetComponent<WeldJoint2DComponent>().joint != nullptr);

	const std::string text = warnings.Text();
	CHECK(Contains(text, "Self's Weld Joint 2D is connected to Self itself, so it has no joint"));
	CHECK(Contains(text, "Lost's Weld Joint 2D is connected to an entity that isn't in the scene, so it has no joint"));
	CHECK(Contains(text, "Plain's Weld Joint 2D is connected to Marker, which has no Rigid Body 2D or tilemap body, so it has no joint"));
	scene.OnRuntimeStop();
}

TEST_CASE("Setting the reference during Play rebuilds the joint, warning and making none for the entity itself")
{
	Scene scene("");
	Entity crate = Welded(scene, "Crate");
	Entity floor = scene.CreateEntity("Floor");
	floor.AddComponent<TransformComponent>();
	floor.AddComponent<RigidBody2DComponent>();
	scene.OnRuntimeStart();

	const Reflection::PropertyPath path = *Reflection::PropertyPath::Parse("WeldJoint2D.ConnectedEntity");
	const WeldJoint2DComponent& weld = crate.GetComponent<WeldJoint2DComponent>();
	b2Body* floorBody = floor.GetComponent<RigidBody2DComponent>().runtimeBody;
	REQUIRE(weld.joint != nullptr);
	CHECK(weld.bodyB != floorBody);

	REQUIRE(Reflection::Set(crate, path, EntityReference{ floor.GetID() }, Reflection::ChangeSource::Lua));
	REQUIRE(weld.joint != nullptr);
	CHECK(weld.bodyB == floorBody);
	CHECK(scene.GetPhysicsEngine2D()->GetWorld()->GetJointCount() == 1);

	WarningCapture warnings;
	REQUIRE(Reflection::Set(crate, path, EntityReference{ crate.GetID() }, Reflection::ChangeSource::Lua));
	CHECK(weld.joint == nullptr);
	CHECK(scene.GetPhysicsEngine2D()->GetWorld()->GetJointCount() == 0);
	CHECK(Contains(warnings.Text(), "Crate's Weld Joint 2D is connected to Crate itself, so it has no joint"));

	// Cleared, it welds to the world again
	REQUIRE(Reflection::Set(crate, path, EntityReference{}, Reflection::ChangeSource::Lua));
	REQUIRE(weld.joint != nullptr);
	CHECK(weld.bodyB != floorBody);
	scene.OnRuntimeStop();
}
