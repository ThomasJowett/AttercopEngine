#include "vendor/doctest.h"
#include "ComponentFixture.h"

#include "Reflection/Reflection.h"
#include "Scene/SceneGraph.h"
#include "Scene/SceneSerializer.h"
#include "Scripting/Lua/LuaManager.h"

#include <string>
#include <vector>

// Scene XML and the play-mode snapshot keep every component's values

namespace
{
	// ComponentFixture::Create saved by the scene XML format of 0.0.3; it must keep loading after format changes
	const char* k_SavedEntity = R"xml(<Entity Name="Root" ID="1001-2002">
    <Transform>
        <Position X="1" Y="2" Z="3"/>
        <Rotation X="0.25" Y="0.5" Z="0.75"/>
        <Scale X="2" Y="3" Z="4"/>
    </Transform>
    <Camera Primary="false" FixedAspectRatio="true">
        <SceneCamera ProjectionType="1" OrthoSize="12" OrthoNear="-2" OrthoFar="3" PerspectiveNear="0.5" PerspectiveFar="250" FOV="0.75" AspectRatio="2"/>
    </Camera>
    <Sprite TilingFactor="3">
        <Texture Filepath="Textures/Brick.png" FilterMethod="0" WrapMethod="1"/>
        <Tint R="0.5" G="0.25" B="0.75" A="0.5"/>
    </Sprite>
    <AnimatedSprite Animation="Run">
        <SpriteSheet Filepath="Sprites/Hero.spritesheet"/>
        <Tint R="0.25" G="0.5" B="0.75" A="1"/>
    </AnimatedSprite>
    <StaticMesh Filepath="Meshes/Rock.staticmesh">
        <Material Slot="2" Filepath="Materials/Moss.material"/>
    </StaticMesh>
    <Primitive Type="5">
        <Material Filepath="Materials/Wall.material"/>
        <Torus OuterRadius="2.5" InnerRadius="0.75" SliceCount="12"/>
    </Primitive>
    <Tilemap Filepath="Tilesets/Grass.tileset" TilesWide="3" TilesHigh="2" TileWidth="16" TileHeight="24" Orientation="Hexagonal" IsTrigger="true" Layer="2" Mask="5">
1,2,3,
4,5,6
<Tint R="0.75" G="0.5" B="0.25" A="1"/></Tilemap>
    <Text MaxWidth="7.5">Hello, world<Font Filepath="Fonts/Title.ttf"/><Colour R="0.25" G="0.75" B="0.5" A="1"/></Text>
    <RigidBody2D BodyType="2" FixedRotation="true" GravityScale="0.5" AngularDamping="0.25" LinearDamping="0.75"/>
    <BoxCollider2D IsTrigger="true" Layer="8" Mask="6">
        <PhysicsMaterial Filepath="Materials/Bouncy.physicsmaterial"/>
        <Offset X="0.5" Y="-0.5"/>
        <Size X="1.5" Y="2.5"/>
    </BoxCollider2D>
    <CircleCollider2D Radius="1.25" IsTrigger="true" Layer="4" Mask="7">
        <PhysicsMaterial Filepath="Materials/Bouncy.physicsmaterial"/>
        <Offset X="-1" Y="1"/>
    </CircleCollider2D>
    <PolygonCollider2D IsTrigger="true" Layer="16" Mask="8">
        <PhysicsMaterial Filepath="Materials/Bouncy.physicsmaterial"/>
        <Offset X="0.25" Y="0.75"/>
        <Vertex X="0" Y="0"/>
        <Vertex X="2" Y="0"/>
        <Vertex X="2" Y="1"/>
        <Vertex X="0" Y="1.5"/>
    </PolygonCollider2D>
    <CapsuleCollider2D Direction="1" Radius="0.75" Height="3.5" IsTrigger="true" Layer="32" Mask="9">
        <PhysicsMaterial Filepath="Materials/Bouncy.physicsmaterial"/>
        <Offset X="1" Y="2"/>
    </CapsuleCollider2D>
    <CircleRenderer Radius="2.5" Thickness="0.25" Fade="0.125">
        <Colour R="1" G="0.5" B="0.25" A="0.75"/>
    </CircleRenderer>
    <BehaviourTree Filepath="AI/Enemy.behaviourtree"/>
    <StateMachine Filepath="AI/Door.statemachine"/>
    <LuaScript Filepath="Scripts/Player.lua"/>
    <PointLight Range="25" Attenuation="2.5" CastShadows="false">
        <Colour R="1" G="0.75" B="0.5" A="1"/>
    </PointLight>
    <Billboard Orientation="0"/>
    <Canvas PixelPerUnit="32"/>
    <Widget Disabled="true" FixedWidth="false" FixedHeight="false" AnchorLeft="0.25" AnchorRight="0.75" AnchorTop="0.125" AnchorBottom="0.875" MarginLeft="-460" MarginRight="-300" MarginTop="-95" MarginBottom="-47" Rotation="0.5">
        <Position X="20" Y="40"/>
        <Size X="160" Y="48"/>
    </Widget>
    <Button>
        <NormalTexture Filepath="Textures/Normal.png" FilterMethod="0" WrapMethod="2"/>
        <HoveredTexture Filepath="Textures/Hovered.png" FilterMethod="1" WrapMethod="0"/>
        <ClickedTexture Filepath="Textures/Clicked.png" FilterMethod="1" WrapMethod="1"/>
        <DisabledTexture Filepath="Textures/Disabled.png" FilterMethod="0" WrapMethod="2"/>
        <NormalTint R="0.5" G="0.5" B="0.5" A="1"/>
        <HoveredTint R="0.75" G="0.75" B="0.75" A="1"/>
        <ClickedTint R="0.25" G="0.25" B="0.25" A="1"/>
        <DisabledTint R="0.5" G="0.5" B="0.5" A="0.5"/>
    </Button>
    <StackLayout Horizontal="true" Spacing="6" StretchCrossAxis="false" PaddingLeft="1" PaddingTop="2" PaddingRight="3" PaddingBottom="4"/>
    <GridLayout Columns="4" UniformCellSize="false" FixedRowHeight="30" PaddingLeft="5" PaddingTop="6" PaddingRight="7" PaddingBottom="8">
        <CellSpacing X="5" Y="7"/>
    </GridLayout>
    <ScrollBox HorizontalScroll="true" VerticalScroll="false" ClipContent="false">
        <ScrollOffset X="10" Y="15"/>
    </ScrollBox>
    <Hidden Hidden="false"/>
    <AudioSource Filepath="Audio/Jump.wav" Volume="0.5" Pitch="1.5" Loop="true" MinDistance="2" MaxDistance="20" Rolloff="0.5" Stream="true" PlayOnStart="true"/>
    <AudioListener Primary="false"/>
    <Entity Name="Child" ID="3003-4004">
        <Transform>
            <Position X="5" Y="6" Z="7"/>
            <Rotation X="0" Y="0" Z="0"/>
            <Scale X="1" Y="1" Z="1"/>
        </Transform>
        <WeldJoint2D CollideConnected="true" Damping="0.5" Stiffness="4"/>
    </Entity>
</Entity>)xml";

	// Every component element with nothing set; a primitive needs the element for its shape
	const char* k_EmptyElements = R"xml(<Entity Name="Defaults" ID="5005-6006">
    <Transform/><Camera/><Sprite/><AnimatedSprite/><StaticMesh/><Primitive><Cube/></Primitive><Tilemap/><Text/><RigidBody2D/>
    <BoxCollider2D/><CircleCollider2D/><PolygonCollider2D/><CapsuleCollider2D/><WeldJoint2D/><CircleRenderer/>
    <BehaviourTree/><StateMachine/><LuaScript/><PointLight/><Billboard/><Canvas/><Widget/><Button/><StackLayout/>
    <GridLayout/><ScrollBox/><Hidden/><AudioSource/><AudioListener/>
</Entity>)xml";

	// The component loaded from an empty element has the values of a newly added one
	template<typename T>
	void CheckDefault(Entity entity, const char* element)
	{
		INFO(element);
		REQUIRE(entity.HasComponent<T>());
		const Reflection::ComponentType& type = Reflection::TypeOf<T>();
		const Reflection::ComponentValues loaded = Reflection::Capture(entity, type);
		const Reflection::ComponentValues defaults = Reflection::Defaults(type);
		CHECK(loaded.values == defaults.values);
		CHECK(loaded.custom == defaults.custom);
	}

	// Play parses the fixture's Lua script, which needs the Lua state Application::Init normally makes
	void StartLua()
	{
		LuaManager::Init();
	}

	void PlayAndStop(Scene& scene)
	{
		StartLua();
		scene.MakeClean();
		scene.OnRuntimeStart();
		scene.OnRuntimeStop();
	}

	std::string RootNames(Scene& scene)
	{
		std::string names;
		for (Entity root : SceneGraph::GetRoots(scene))
			names += (names.empty() ? "" : " ") + root.GetName();
		return names;
	}
}

TEST_CASE("Every component survives being saved to scene XML and loaded")
{
	Scene source("");
	std::string xml = SceneSerializer::SerializeEntity(ComponentFixture::Create(source));

	Scene loaded("");
	ComponentFixture::Check(SceneSerializer::DeserializeEntity(&loaded, xml));
}

TEST_CASE("Scene XML saved by this version still loads")
{
	Scene scene("");
	ComponentFixture::Check(SceneSerializer::DeserializeEntity(&scene, k_SavedEntity));
}

TEST_CASE("Saving a loaded entity writes the same XML")
{
	Scene source("");
	std::string first = SceneSerializer::SerializeEntity(ComponentFixture::Create(source));

	Scene loaded("");
	std::string second = SceneSerializer::SerializeEntity(SceneSerializer::DeserializeEntity(&loaded, first));
	CHECK(second == first);
}

TEST_CASE("Play then Stop keeps every entity's handle, ID and values")
{
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);
	Entity first = scene.CreateEntity("First");
	SceneGraph::MoveBefore(first, root);
	std::vector<Entity> entities = { root, root.GetChild(), first };
	std::vector<Uuid> ids;
	for (Entity entity : entities)
		ids.push_back(entity.GetID());

	PlayAndStop(scene);

	for (size_t i = 0; i < entities.size(); ++i)
	{
		CAPTURE(i);
		REQUIRE(scene.GetRegistry().valid(entities[i].GetHandle()));
		CHECK(entities[i].GetID() == ids[i]);
		CHECK(scene.GetEntityByID(ids[i]).GetHandle() == entities[i].GetHandle());
	}
	CHECK(entities[1].GetParent() == root);
	CHECK(RootNames(scene) == "First Root");
	ComponentFixture::Check(root);
}

TEST_CASE("Stop rebuilds runtime state rather than copying it")
{
	StartLua();
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);
	scene.MakeClean();

	scene.OnRuntimeStart();
	REQUIRE((b2Body*)root.GetComponent<RigidBody2DComponent>().runtimeBody != nullptr);
	CHECK((b2Body*)root.GetComponent<BoxCollider2DComponent>().runtimeBody != nullptr);
	scene.OnRuntimeStop();

	CHECK((b2Body*)root.GetComponent<RigidBody2DComponent>().runtimeBody == nullptr);
	CHECK((b2Body*)root.GetComponent<BoxCollider2DComponent>().runtimeBody == nullptr);
	CHECK((b2Body*)root.GetComponent<CircleCollider2DComponent>().runtimeBody == nullptr);
	CHECK(root.GetComponent<TilemapComponent>().runtimeBody == nullptr);
	CHECK_FALSE(root.GetComponent<AudioSourceComponent>().sound);
}

TEST_CASE("Stop removes entities made during Play and brings back ones destroyed, with their handles")
{
	StartLua();
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);
	Entity child = root.GetChild();
	Entity last = scene.CreateEntity("Last");
	const entt::entity childHandle = child.GetHandle(), lastHandle = last.GetHandle();
	const Uuid childID = child.GetID(), lastID = last.GetID();
	scene.MakeClean();

	scene.OnRuntimeStart();
	Entity spawned = scene.CreateEntity("Spawned");
	const entt::entity spawnedHandle = spawned.GetHandle();
	const Uuid spawnedID = spawned.GetID();
	scene.RemoveEntity(child);
	scene.RemoveEntity(last);
	scene.OnRuntimeStop();

	CHECK_FALSE(scene.GetEntityByID(spawnedID));
	CHECK_FALSE(scene.GetRegistry().valid(spawnedHandle));
	REQUIRE(scene.GetRegistry().valid(childHandle));
	CHECK(scene.GetEntityByID(childID).GetHandle() == childHandle);
	CHECK(scene.GetEntityByID(lastID).GetHandle() == lastHandle);
	CHECK(Entity(childHandle, &scene).GetParent() == root);
	CHECK(RootNames(scene) == "Root Last");
	ComponentFixture::Check(root);
}

TEST_CASE("Every primitive shape keeps its own settings")
{
	struct Shape { const char* name; std::function<void(PrimitiveComponent&)> set; std::function<void(const PrimitiveComponent&)> check; };
	const std::vector<Shape> shapes = {
		{ "Cube", [](PrimitiveComponent& c) { c.SetCube(2.0f, 4.0f, 3.0f); },
			[](const PrimitiveComponent& c) { CHECK(c.cubeWidth == 2.0f); CHECK(c.cubeHeight == 3.0f); CHECK(c.cubeDepth == 4.0f); } },
		{ "Sphere", [](PrimitiveComponent& c) { c.SetSphere(1.5f, 8, 12); },
			[](const PrimitiveComponent& c) { CHECK(c.sphereRadius == 1.5f); CHECK(c.sphereLongitudeLines == 8); CHECK(c.sphereLatitudeLines == 12); } },
		{ "Plane", [](PrimitiveComponent& c) { c.SetPlane(4.0f, 6.0f, 3, 5, 2.0f, 0.5f); },
			[](const PrimitiveComponent& c) {
				CHECK(c.planeWidth == 4.0f); CHECK(c.planeLength == 6.0f); CHECK(c.planeWidthLines == 3);
				CHECK(c.planeLengthLines == 5); CHECK(c.planeTileU == 2.0f); CHECK(c.planeTileV == 0.5f); } },
		{ "Cylinder", [](PrimitiveComponent& c) { c.SetCylinder(1.0f, 0.5f, 3.0f, 10, 4); },
			[](const PrimitiveComponent& c) {
				CHECK(c.cylinderBottomRadius == 1.0f); CHECK(c.cylinderTopRadius == 0.5f); CHECK(c.cylinderHeight == 3.0f);
				CHECK(c.cylinderSliceCount == 10); CHECK(c.cylinderStackCount == 4); } },
		{ "Cone", [](PrimitiveComponent& c) { c.SetCone(1.25f, 2.5f, 9, 3); },
			[](const PrimitiveComponent& c) {
				CHECK(c.coneBottomRadius == 1.25f); CHECK(c.coneHeight == 2.5f); CHECK(c.coneSliceCount == 9); CHECK(c.coneStackCount == 3); } },
		{ "Torus", [](PrimitiveComponent& c) { c.SetTorus(3.0f, 0.5f, 20); },
			[](const PrimitiveComponent& c) { CHECK(c.torusOuterRadius == 3.0f); CHECK(c.torusInnerRadius == 0.5f); CHECK(c.torusSliceCount == 20); } },
	};

	for (const Shape& shape : shapes)
	{
		CAPTURE(shape.name);
		Scene source("");
		Entity entity = source.CreateEntity("Shape");
		PrimitiveComponent& primitive = entity.AddComponent<PrimitiveComponent>();
		shape.set(primitive);
		PrimitiveComponent::Shape type = primitive.type;

		Scene loaded("");
		Entity fromXml = SceneSerializer::DeserializeEntity(&loaded, SceneSerializer::SerializeEntity(entity));
		REQUIRE(fromXml.HasComponent<PrimitiveComponent>());
		CHECK(fromXml.GetComponent<PrimitiveComponent>().type == type);
		shape.check(fromXml.GetComponent<PrimitiveComponent>());

		PlayAndStop(source);
		REQUIRE(entity.HasComponent<PrimitiveComponent>());
		CHECK(entity.GetComponent<PrimitiveComponent>().type == type);
		shape.check(entity.GetComponent<PrimitiveComponent>());
	}
}

TEST_CASE("A duplicated entity gets new IDs and keeps everything else")
{
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);
	Entity copy = scene.DuplicateEntity(root, Entity());

	REQUIRE(copy);
	CHECK(copy != root);
	CHECK_FALSE(copy.GetID() == root.GetID());
	REQUIRE(copy.GetChild());
	CHECK_FALSE(copy.GetChild().GetID() == root.GetChild().GetID());

	// Load the copy into another scene with the original IDs, then every other value must match
	std::string xml = SceneSerializer::SerializeEntity(copy);
	for (auto [from, to] : { std::pair(copy.GetID(), root.GetID()), std::pair(copy.GetChild().GetID(), root.GetChild().GetID()) })
		xml.replace(xml.find(from.to_string()), from.to_string().size(), to.to_string());
	Scene restored("");
	ComponentFixture::Check(SceneSerializer::DeserializeEntity(&restored, xml));
}

TEST_CASE("A component saved with nothing set loads with its defaults")
{
	Scene scene("");
	Entity entity = SceneSerializer::DeserializeEntity(&scene, k_EmptyElements);
	REQUIRE(entity);

	CheckDefault<TransformComponent>(entity, "Transform");
	CheckDefault<CameraComponent>(entity, "Camera");
	CheckDefault<SpriteComponent>(entity, "Sprite");
	CheckDefault<AnimatedSpriteComponent>(entity, "AnimatedSprite");
	CheckDefault<StaticMeshComponent>(entity, "StaticMesh");
	CheckDefault<PrimitiveComponent>(entity, "Primitive");
	CheckDefault<TilemapComponent>(entity, "Tilemap");
	CheckDefault<TextComponent>(entity, "Text");
	CheckDefault<RigidBody2DComponent>(entity, "RigidBody2D");
	CheckDefault<BoxCollider2DComponent>(entity, "BoxCollider2D");
	CheckDefault<CircleCollider2DComponent>(entity, "CircleCollider2D");
	CheckDefault<PolygonCollider2DComponent>(entity, "PolygonCollider2D");
	CheckDefault<CapsuleCollider2DComponent>(entity, "CapsuleCollider2D");
	CheckDefault<WeldJoint2DComponent>(entity, "WeldJoint2D");
	CheckDefault<CircleRendererComponent>(entity, "CircleRenderer");
	CheckDefault<BehaviourTreeComponent>(entity, "BehaviourTree");
	CheckDefault<StateMachineComponent>(entity, "StateMachine");
	CheckDefault<LuaScriptComponent>(entity, "LuaScript");
	CheckDefault<PointLightComponent>(entity, "PointLight");
	CheckDefault<BillboardComponent>(entity, "Billboard");
	CheckDefault<CanvasComponent>(entity, "Canvas");
	CheckDefault<WidgetComponent>(entity, "Widget");
	CheckDefault<ButtonComponent>(entity, "Button");
	CheckDefault<StackLayoutComponent>(entity, "StackLayout");
	CheckDefault<GridLayoutComponent>(entity, "GridLayout");
	CheckDefault<ScrollBoxComponent>(entity, "ScrollBox");
	CheckDefault<HiddenComponent>(entity, "Hidden");
	CheckDefault<AudioSourceComponent>(entity, "AudioSource");
	CheckDefault<AudioListenerComponent>(entity, "AudioListener");
}
