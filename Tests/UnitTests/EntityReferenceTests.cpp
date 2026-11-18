#include "vendor/doctest.h"
#include "TestHelpers.h"

#include "Core/UUID.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneGraph.h"
#include "Scene/SceneSerializer.h"

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
