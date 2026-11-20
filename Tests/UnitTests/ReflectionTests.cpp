#include "vendor/doctest.h"

#include "ComponentFixture.h"

#include "Reflection/Describe.h"
#include "Reflection/Reflection.h"
#include "Reflection/SceneXml.h"
#include "History/HistoryCommands.h"
#include "Inspector/InspectorWidgets.h"
#include "Inspector/PropertyEdits.h"
#include "Physics/CollisionLayers.h"
#include "Scene/Components.h"
#include "Scene/SceneGraph.h"
#include "Scene/SceneSerializer.h"
#include "Utilities/GeometryGenerator.h"

#include "TinyXml2/tinyxml2.h"

#include <cfloat>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace ReflectionTests
{
	using namespace Reflection;

	// Counts its hooks, so tests can check how often Set and Restore run them
	struct HookCounter
	{
		float speed = 1.0f;
		float scale = 1.0f;
		int32_t count = 0;
		std::vector<Vector2f> points = { Vector2f(0.0f, 0.0f), Vector2f(1.0f, 2.0f) };

		inline static int s_FieldHooks = 0;
		inline static int s_ComponentHooks = 0;

		static int32_t Total(const HookCounter& c) { return c.count * 2; }
		static void OnFieldChanged(Entity, HookCounter&) { ++s_FieldHooks; }
		static void OnComponentChanged(Entity, HookCounter&) { ++s_ComponentHooks; }

		static void Describe(Builder<HookCounter>& c)
		{
			c.Name("ReflectionTestHookCounter").Category("Tests").Description("Counts hooks");
			c.Field<&HookCounter::speed>("Speed", "Shares its hook with Scale").OnChanged(&OnFieldChanged);
			c.Field<&HookCounter::scale>("Scale", "Shares its hook with Speed").OnChanged(&OnFieldChanged);
			c.Field<&HookCounter::count>("Count", "No hook of its own");
			c.Field<&HookCounter::points>("Points", "A list").Item("Point");
			c.Property<&HookCounter::Total>("Total", "Read-only");
			c.OnChanged(&OnComponentChanged);
		}

		static void ResetHooks() { s_FieldHooks = 0; s_ComponentHooks = 0; }
	};

	void Setup()
	{
		RegisterComponents();
		Register<HookCounter>();
	}

	PropertyPath Path(const char* text)
	{
		std::optional<PropertyPath> path = PropertyPath::Parse(text);
		INFO(text);
		REQUIRE(path.has_value());
		return *path;
	}

	std::string Text(const std::optional<Value>& value) { return value ? value->ToString() : std::string("nothing"); }

	AssetValue AssetPath(const char* path) { return { path, nullptr }; }

	template<typename E>
	EnumValue EnumOfValue(E value) { return { &EnumOf<E>(), (int32_t)value }; }

	struct Expected
	{
		const char* path;
		Value value;
	};

	// Every described field of the fixture's root entity
	std::vector<Expected> RootValues()
	{
		using Shape = PrimitiveComponent::Shape;
		return {
			{ "ID.ID", std::string(ComponentFixture::k_RootId) },
			{ "Name.Name", "Root" },
			{ "Transform.Position", Vector3f(1.0f, 2.0f, 3.0f) },
			{ "Transform.Rotation", Vector3f(0.25f, 0.5f, 0.75f) },
			{ "Transform.Scale", Vector3f(2.0f, 3.0f, 4.0f) },
			{ "Camera.Primary", false },
			{ "Camera.FixedAspectRatio", true },
			{ "Camera.Projection", EnumOfValue(SceneCamera::ProjectionType::orthographic) },
			{ "Camera.OrthoSize", 12.0f },
			{ "Camera.OrthoNear", -2.0f },
			{ "Camera.OrthoFar", 3.0f },
			{ "Camera.PerspectiveNear", 0.5f },
			{ "Camera.PerspectiveFar", 250.0f },
			{ "Camera.Fov", 0.75f },
			{ "Camera.AspectRatio", 2.0f },
			{ "Sprite.TilingFactor", 3.0f },
			{ "Sprite.FilterMethod", EnumOfValue(Texture::FilterMethod::Linear) },
			{ "Sprite.WrapMethod", EnumOfValue(Texture::WrapMethod::Mirror) },
			{ "Sprite.Tint", Colour(0.5f, 0.25f, 0.75f, 0.5f) },
			{ "Sprite.Texture", AssetPath("Textures/Brick.png") },
			{ "AnimatedSprite.SpriteSheet", AssetPath("Sprites/Hero.spritesheet") },
			{ "AnimatedSprite.Tint", Colour(0.25f, 0.5f, 0.75f, 1.0f) },
			{ "AnimatedSprite.Animation", "Run" },
			{ "StaticMesh.Mesh", AssetPath("Meshes/Rock.staticmesh") },
			{ "Primitive.Type", EnumOfValue(Shape::Torus) },
			{ "Primitive.Material", AssetPath("Materials/Wall.material") },
			{ "Primitive.CubeWidth", 1.0f },
			{ "Primitive.CubeHeight", 1.0f },
			{ "Primitive.CubeDepth", 1.0f },
			{ "Primitive.SphereRadius", 0.5f },
			{ "Primitive.SphereLongitudeLines", 16u },
			{ "Primitive.SphereLatitudeLines", 32u },
			{ "Primitive.PlaneWidth", 1.0f },
			{ "Primitive.PlaneLength", 1.0f },
			{ "Primitive.PlaneWidthLines", 2u },
			{ "Primitive.PlaneLengthLines", 2u },
			{ "Primitive.PlaneTileU", 1.0f },
			{ "Primitive.PlaneTileV", 1.0f },
			{ "Primitive.CylinderBottomRadius", 0.5f },
			{ "Primitive.CylinderTopRadius", 0.5f },
			{ "Primitive.CylinderHeight", 1.0f },
			{ "Primitive.CylinderSliceCount", 32u },
			{ "Primitive.CylinderStackCount", 5u },
			{ "Primitive.ConeBottomRadius", 0.5f },
			{ "Primitive.ConeHeight", 1.0f },
			{ "Primitive.ConeSliceCount", 32u },
			{ "Primitive.ConeStackCount", 5u },
			{ "Primitive.TorusOuterRadius", 2.5f },
			{ "Primitive.TorusInnerRadius", 0.75f },
			{ "Primitive.TorusSliceCount", 12u },
			{ "Tilemap.Tileset", AssetPath("Tilesets/Grass.tileset") },
			{ "Tilemap.TilesWide", 3u },
			{ "Tilemap.TilesHigh", 2u },
			{ "Tilemap.TileWidth", 16u },
			{ "Tilemap.TileHeight", 24u },
			{ "Tilemap.Tint", Colour(0.75f, 0.5f, 0.25f, 1.0f) },
			{ "Tilemap.Orientation", EnumOfValue(TilemapComponent::Orientation::hexagonal) },
			{ "Tilemap.IsTrigger", true },
			{ "Tilemap.Layer", LayerValue{ 2 } },
			{ "Tilemap.Mask", MaskValue{ 5 } },
			{ "Text.Text", "Hello, world" },
			{ "Text.MaxWidth", 7.5f },
			{ "Text.Colour", Colour(0.25f, 0.75f, 0.5f, 1.0f) },
			{ "Text.Font", AssetPath("Fonts/Title.ttf") },
			{ "RigidBody2D.Type", EnumOfValue(RigidBody2DComponent::BodyType::DYNAMIC) },
			{ "RigidBody2D.FixedRotation", true },
			{ "RigidBody2D.GravityScale", 0.5f },
			{ "RigidBody2D.AngularDamping", 0.25f },
			{ "RigidBody2D.LinearDamping", 0.75f },
			{ "BoxCollider2D.PhysicsMaterial", AssetPath("Materials/Bouncy.physicsmaterial") },
			{ "BoxCollider2D.Offset", Vector2f(0.5f, -0.5f) },
			{ "BoxCollider2D.Size", Vector2f(1.5f, 2.5f) },
			{ "BoxCollider2D.IsTrigger", true },
			{ "BoxCollider2D.Layer", LayerValue{ 8 } },
			{ "BoxCollider2D.Mask", MaskValue{ 6 } },
			{ "CircleCollider2D.PhysicsMaterial", AssetPath("Materials/Bouncy.physicsmaterial") },
			{ "CircleCollider2D.Radius", 1.25f },
			{ "CircleCollider2D.Offset", Vector2f(-1.0f, 1.0f) },
			{ "CircleCollider2D.IsTrigger", true },
			{ "CircleCollider2D.Layer", LayerValue{ 4 } },
			{ "CircleCollider2D.Mask", MaskValue{ 7 } },
			{ "PolygonCollider2D.PhysicsMaterial", AssetPath("Materials/Bouncy.physicsmaterial") },
			{ "PolygonCollider2D.Offset", Vector2f(0.25f, 0.75f) },
			{ "PolygonCollider2D.Vertices", ValueList{ Vector2f(0.0f, 0.0f), Vector2f(2.0f, 0.0f), Vector2f(2.0f, 1.0f), Vector2f(0.0f, 1.5f) } },
			{ "PolygonCollider2D.IsTrigger", true },
			{ "PolygonCollider2D.Layer", LayerValue{ 16 } },
			{ "PolygonCollider2D.Mask", MaskValue{ 8 } },
			{ "CapsuleCollider2D.PhysicsMaterial", AssetPath("Materials/Bouncy.physicsmaterial") },
			{ "CapsuleCollider2D.Direction", EnumOfValue(CapsuleCollider2DComponent::Direction::Horizontal) },
			{ "CapsuleCollider2D.Radius", 0.75f },
			{ "CapsuleCollider2D.Height", 3.5f },
			{ "CapsuleCollider2D.Offset", Vector2f(1.0f, 2.0f) },
			{ "CapsuleCollider2D.IsTrigger", true },
			{ "CapsuleCollider2D.Layer", LayerValue{ 32 } },
			{ "CapsuleCollider2D.Mask", MaskValue{ 9 } },
			{ "CircleRenderer.Radius", 2.5f },
			{ "CircleRenderer.Thickness", 0.25f },
			{ "CircleRenderer.Fade", 0.125f },
			{ "CircleRenderer.Colour", Colour(1.0f, 0.5f, 0.25f, 0.75f) },
			{ "BehaviourTree.Filepath", AssetPath("AI/Enemy.behaviourtree") },
			{ "StateMachine.Filepath", AssetPath("AI/Door.statemachine") },
			{ "LuaScript.Script", AssetPath("Scripts/Player.lua") },
			{ "PointLight.Range", 25.0f },
			{ "PointLight.Attenuation", 2.5f },
			{ "PointLight.CastShadows", false },
			{ "PointLight.Colour", Colour(1.0f, 0.75f, 0.5f, 1.0f) },
			{ "Billboard.Orientation", EnumOfValue(BillboardComponent::Orientation::WorldUp) },
			{ "Canvas.PixelPerUnit", 32.0f },
			{ "Widget.Disabled", true },
			{ "Widget.FixedWidth", false },
			{ "Widget.FixedHeight", false },
			{ "Widget.Position", Vector2f(20.0f, 40.0f) },
			{ "Widget.Size", Vector2f(160.0f, 48.0f) },
			{ "Widget.AnchorLeft", 0.25f },
			{ "Widget.AnchorRight", 0.75f },
			{ "Widget.AnchorTop", 0.125f },
			{ "Widget.AnchorBottom", 0.875f },
			{ "Widget.MarginLeft", -460.0f },
			{ "Widget.MarginRight", -300.0f },
			{ "Widget.MarginTop", -95.0f },
			{ "Widget.MarginBottom", -47.0f },
			{ "Widget.Rotation", 0.5f },
			{ "Button.NormalTexture", AssetPath("Textures/Normal.png") },
			{ "Button.HoveredTexture", AssetPath("Textures/Hovered.png") },
			{ "Button.ClickedTexture", AssetPath("Textures/Clicked.png") },
			{ "Button.DisabledTexture", AssetPath("Textures/Disabled.png") },
			{ "Button.NormalFilterMethod", EnumOfValue(Texture::FilterMethod::Linear) },
			{ "Button.HoveredFilterMethod", EnumOfValue(Texture::FilterMethod::Nearest) },
			{ "Button.ClickedFilterMethod", EnumOfValue(Texture::FilterMethod::Nearest) },
			{ "Button.DisabledFilterMethod", EnumOfValue(Texture::FilterMethod::Linear) },
			{ "Button.NormalWrapMethod", EnumOfValue(Texture::WrapMethod::Repeat) },
			{ "Button.HoveredWrapMethod", EnumOfValue(Texture::WrapMethod::Clamp) },
			{ "Button.ClickedWrapMethod", EnumOfValue(Texture::WrapMethod::Mirror) },
			{ "Button.DisabledWrapMethod", EnumOfValue(Texture::WrapMethod::Repeat) },
			{ "Button.NormalTint", Colour(0.5f, 0.5f, 0.5f, 1.0f) },
			{ "Button.HoveredTint", Colour(0.75f, 0.75f, 0.75f, 1.0f) },
			{ "Button.ClickedTint", Colour(0.25f, 0.25f, 0.25f, 1.0f) },
			{ "Button.DisabledTint", Colour(0.5f, 0.5f, 0.5f, 0.5f) },
			{ "StackLayout.Horizontal", true },
			{ "StackLayout.Spacing", 6.0f },
			{ "StackLayout.StretchCrossAxis", false },
			{ "StackLayout.PaddingLeft", 1.0f },
			{ "StackLayout.PaddingTop", 2.0f },
			{ "StackLayout.PaddingRight", 3.0f },
			{ "StackLayout.PaddingBottom", 4.0f },
			{ "GridLayout.Columns", 4 },
			{ "GridLayout.CellSpacing", Vector2f(5.0f, 7.0f) },
			{ "GridLayout.UniformCellSize", false },
			{ "GridLayout.FixedRowHeight", 30.0f },
			{ "GridLayout.PaddingLeft", 5.0f },
			{ "GridLayout.PaddingTop", 6.0f },
			{ "GridLayout.PaddingRight", 7.0f },
			{ "GridLayout.PaddingBottom", 8.0f },
			{ "ScrollBox.ScrollOffset", Vector2f(10.0f, 15.0f) },
			{ "ScrollBox.HorizontalScroll", true },
			{ "ScrollBox.VerticalScroll", false },
			{ "ScrollBox.ClipContent", false },
			{ "Hidden.Hidden", false },
			{ "AudioSource.Clip", AssetPath("Audio/Jump.wav") },
			{ "AudioSource.Volume", 0.5f },
			{ "AudioSource.Pitch", 1.5f },
			{ "AudioSource.Loop", true },
			{ "AudioSource.MinDistance", 2.0f },
			{ "AudioSource.MaxDistance", 20.0f },
			{ "AudioSource.Rolloff", 0.5f },
			{ "AudioSource.Stream", true },
			{ "AudioSource.PlayOnStart", true },
			{ "AudioListener.Primary", false },
		};
	}

	std::vector<Expected> ChildValues()
	{
		return {
			{ "ID.ID", std::string(ComponentFixture::k_ChildId) },
			{ "Name.Name", "Child" },
			{ "Transform.Position", Vector3f(5.0f, 6.0f, 7.0f) },
			{ "Transform.Rotation", Vector3f(0.0f, 0.0f, 0.0f) },
			{ "Transform.Scale", Vector3f(1.0f, 1.0f, 1.0f) },
			{ "WeldJoint2D.CollideConnected", true },
			{ "WeldJoint2D.Damping", 0.5f },
			{ "WeldJoint2D.Stiffness", 4.0f },
		};
	}

	void CheckValues(Entity entity, const std::vector<Expected>& expected)
	{
		for (const Expected& property : expected)
		{
			INFO(property.path);
			std::optional<Value> value = Get(entity, Path(property.path));
			CHECK_MESSAGE((value && *value == property.value), (Text(value) + " should be " + property.value.ToString()));
		}
	}

	bool IsTestComponent(const ComponentType* type) { return type == &TypeOf<HookCounter>(); }

	Entity Child(Entity root) { return root.GetChild(); }

	// The fixture entity that has the component
	Entity Holder(Entity root, const ComponentType& type)
	{
		entt::registry& registry = root.GetScene()->GetRegistry();
		return type.tryGet(registry, root.GetHandle()) ? root : Child(root);
	}

	struct ChangeRecorder
	{
		std::vector<PropertyChanged>* changes;
		void OnChanged(const PropertyChanged& change) { changes->push_back(change); }
	};

	// The names separated by spaces, for test failure messages
	std::string Names(const std::vector<std::string>& names)
	{
		std::string text;
		for (const std::string& name : names)
			text += (text.empty() ? "" : " ") + name;
		return text;
	}

TEST_CASE("Every component is registered, in the fixture, and reads back through Get")
{
	Setup();

	std::vector<std::string> names;
	for (const ComponentType* type : Components())
	{
		if (!IsTestComponent(type))
			names.push_back(type->name);
	}
	// Today's SerializeEntity order, with the entity's attributes first and its children last
	const std::vector<std::string> expected = { "ID", "Name", "Transform", "Camera", "Sprite", "AnimatedSprite", "StaticMesh",
		"Primitive", "Tilemap", "Text", "RigidBody2D", "BoxCollider2D", "CircleCollider2D", "PolygonCollider2D", "CapsuleCollider2D",
		"WeldJoint2D", "CircleRenderer", "BehaviourTree", "StateMachine", "LuaScript", "PointLight", "Billboard", "Canvas",
		"Widget", "Button", "StackLayout", "GridLayout", "ScrollBox", "Hidden", "AudioSource", "AudioListener", "Hierarchy" };
	CHECK_MESSAGE(names == expected, Names(names));

	CHECK(TypeOf<IDComponent>().IsInternal());
	CHECK(TypeOf<NameComponent>().IsInternal());
	CHECK(TypeOf<HierarchyComponent>().IsInternal());
	CHECK_FALSE(TypeOf<SpriteComponent>().IsInternal());
	CHECK_FALSE(TypeOf<TransformComponent>().Has(ComponentFlags::Required));
	CHECK(TypeOf<SpriteComponent>().luaName == "SpriteComponent");
	CHECK(FindComponent("RigidBody2D") == &TypeOf<RigidBody2DComponent>());
	CHECK(FindComponent("RigidBody2DComponent") == nullptr);

	Scene scene("");
	Entity root = ComponentFixture::Create(scene);

	std::set<std::string> described;
	for (const ComponentType* type : Components())
	{
		if (IsTestComponent(type))
			continue;
		INFO(type->name);
		Entity holder = Holder(root, *type);
		CHECK(type->tryGet(scene.GetRegistry(), holder.GetHandle()) != nullptr);
		CHECK_FALSE(type->description.empty());
		CHECK_FALSE(type->category.empty());

		for (const FieldInfo& field : type->fields)
		{
			CHECK_FALSE(field.description.empty());
			if (!field.IsCustom())
				described.insert((holder == root ? "" : "Child ") + type->name + "." + field.name);
		}
	}

	CheckValues(root, RootValues());
	CheckValues(Child(root), ChildValues());

	// The tables cover every field that isn't custom
	std::set<std::string> checked;
	for (const Expected& property : RootValues())
		checked.insert(property.path);
	for (const Expected& property : ChildValues())
	{
		// Transform, ID and Name are on both
		std::string path = property.path;
		if (path.rfind("WeldJoint2D.", 0) == 0)
			path = "Child " + path;
		checked.insert(path);
	}
	for (const std::string& path : described)
	{
		INFO(path);
		CHECK(checked.count(path) == 1);
	}
}

TEST_CASE("Descriptions record their options")
{
	Setup();

	const ComponentType& primitive = TypeOf<PrimitiveComponent>();
	const FieldInfo& outerRadius = primitive.fields[primitive.FindField("TorusOuterRadius")];
	CHECK(outerRadius.xmlName == "OuterRadius");
	REQUIRE(outerRadius.group >= 0);
	CHECK(primitive.groups[outerRadius.group].name == "Torus");
	CHECK(primitive.FindField("OuterRadius") < 0);

	PrimitiveComponent shape;
	shape.type = PrimitiveComponent::Shape::Torus;
	CHECK(primitive.groups[outerRadius.group].when(&shape));
	shape.type = PrimitiveComponent::Shape::Cube;
	CHECK_FALSE(primitive.groups[outerRadius.group].when(&shape));

	const ComponentType& sprite = TypeOf<SpriteComponent>();
	CHECK(sprite.fields[sprite.FindField("TilingFactor")].label == "Tiling Factor");
	CHECK(sprite.fields[sprite.FindField("Texture")].assetType == "Texture2D");
	CHECK(sprite.fields[sprite.FindField("FilterMethod")].enumType == &EnumOf<Texture::FilterMethod>());

	const ComponentType& polygon = TypeOf<PolygonCollider2DComponent>();
	const FieldInfo& vertices = polygon.fields[polygon.FindField("Vertices")];
	CHECK(vertices.type == ValueType::List);
	CHECK(vertices.itemType == ValueType::Vec2);
	CHECK(vertices.itemName == "Vertex");

	CHECK(TypeOf<StaticMeshComponent>().fields[0].Has(FieldFlags::MainAsset));
	CHECK(TypeOf<StaticMeshComponent>().fields[1].IsCustom());
	CHECK(TypeOf<TilemapComponent>().fields[TypeOf<TilemapComponent>().FindField("Tiles")].IsCustom());
	CHECK(TypeOf<TextComponent>().fields[0].Has(FieldFlags::AsText));
	CHECK(TypeOf<IDComponent>().fields[0].Has(FieldFlags::ReadOnly));
	CHECK(TypeOf<BehaviourTreeComponent>().fields[0].assetType == "BehaviourTree");

	const ComponentType& rigidBody = TypeOf<RigidBody2DComponent>();
	RigidBody2DComponent body;
	CHECK_FALSE(rigidBody.fields[rigidBody.FindField("GravityScale")].showIf(&body));
	body.type = RigidBody2DComponent::BodyType::DYNAMIC;
	CHECK(rigidBody.fields[rigidBody.FindField("GravityScale")].showIf(&body));
	CHECK(rigidBody.fields[rigidBody.FindField("Type")].showIf(&body));
}

TEST_CASE("Property paths parse and print back")
{
	Setup();

	for (const char* text : { "Transform.Position", "Transform.Position.X", "Sprite.Tint.A", "PolygonCollider2D.Vertices[2]",
		"PolygonCollider2D.Vertices[2].Y", "Primitive.TorusOuterRadius", "Name.Name" })
	{
		INFO(text);
		std::optional<PropertyPath> path = PropertyPath::Parse(text);
		REQUIRE(path.has_value());
		CHECK(path->ToString() == text);
		CHECK(PropertyPath::Parse(path->ToString()) == path);
	}

	PropertyPath part = Path("Transform.Position.Z");
	CHECK(part.component == &TypeOf<TransformComponent>());
	CHECK(part.Field().name == "Position");
	CHECK(part.index == -1);
	CHECK(part.part == 2);
	CHECK(part.Type() == ValueType::Float);

	PropertyPath item = Path("PolygonCollider2D.Vertices[3]");
	CHECK(item.index == 3);
	CHECK(item.part == -1);
	CHECK(item.Type() == ValueType::Vec2);
	CHECK(Path("PolygonCollider2D.Vertices").Type() == ValueType::List);
	CHECK(Path("Sprite.Tint.G").part == 1);

	for (const char* text : { "Nope.Position", "Transform.Nope", "Transform", "Transform.Position.W", "Sprite.Tint.X",
		"Sprite.Tint[0]", "PolygonCollider2D.Vertices[x]", "PolygonCollider2D.Vertices[]", "PolygonCollider2D.Vertices[1",
		"PolygonCollider2D.Vertices[-1]", "Transform.Position.X.Y", "Primitive.OuterRadius", "Camera.Primary.X", "" })
	{
		INFO(text);
		CHECK_FALSE(PropertyPath::Parse(text).has_value());
	}
}

TEST_CASE("Get reads parts of vectors and list items")
{
	Setup();
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);

	CHECK(Get(root, Path("Transform.Position.Y")) == Value(2.0f));
	CHECK(Get(root, Path("Sprite.Tint.A")) == Value(0.5f));
	CHECK(Get(root, Path("PolygonCollider2D.Vertices[1]")) == Value(Vector2f(2.0f, 0.0f)));
	CHECK(Get(root, Path("PolygonCollider2D.Vertices[3].Y")) == Value(1.5f));
	CHECK_FALSE(Get(root, Path("PolygonCollider2D.Vertices[4]")).has_value());
	// The child has no sprite
	CHECK_FALSE(Get(Child(root), Path("Sprite.Tint")).has_value());
}

TEST_CASE("Set changes the field, runs OnChanged once and sends one PropertyChanged")
{
	Setup();
	Scene scene("");
	Entity entity = scene.CreateEntity("Counter");
	HookCounter& counter = scene.GetRegistry().emplace<HookCounter>(entity.GetHandle());

	std::vector<PropertyChanged> changes;
	ChangeRecorder listener{ &changes };
	scene.OnPropertyChanged().connect<&ChangeRecorder::OnChanged>(listener);

	HookCounter::ResetHooks();
	scene.MakeClean();
	REQUIRE(Set(entity, Path("ReflectionTestHookCounter.Speed"), 3.0f, ChangeSource::Editor));
	CHECK(counter.speed == 3.0f);
	CHECK(HookCounter::s_FieldHooks == 1);
	CHECK(HookCounter::s_ComponentHooks == 1);
	CHECK(scene.IsDirty());
	REQUIRE(changes.size() == 1);
	CHECK(changes[0].entity == entity.GetID());
	CHECK(changes[0].path == Path("ReflectionTestHookCounter.Speed"));
	CHECK(changes[0].oldValue == Value(1.0f));
	CHECK(changes[0].newValue == Value(3.0f));
	CHECK(changes[0].source == ChangeSource::Editor);

	SUBCASE("An equal value does nothing")
	{
		HookCounter::ResetHooks();
		changes.clear();
		scene.MakeClean();
		CHECK(Set(entity, Path("ReflectionTestHookCounter.Speed"), 3.0f, ChangeSource::Lua));
		CHECK(HookCounter::s_FieldHooks == 0);
		CHECK(HookCounter::s_ComponentHooks == 0);
		CHECK(changes.empty());
		CHECK_FALSE(scene.IsDirty());
	}

	SUBCASE("A wrong type does nothing")
	{
		HookCounter::ResetHooks();
		changes.clear();
		CHECK_FALSE(Set(entity, Path("ReflectionTestHookCounter.Speed"), 4, ChangeSource::Lua));
		CHECK_FALSE(Set(entity, Path("ReflectionTestHookCounter.Count"), 4.0f, ChangeSource::Lua));
		CHECK_FALSE(Set(entity, Path("ReflectionTestHookCounter.Points"), Vector2f(1.0f, 1.0f), ChangeSource::Lua));
		CHECK_FALSE(Set(entity, Path("ReflectionTestHookCounter.Points"), ValueList{ 1.0f }, ChangeSource::Lua));
		CHECK_FALSE(Set(entity, Path("ReflectionTestHookCounter.Points[0].X"), 2, ChangeSource::Lua));
		CHECK(counter.speed == 3.0f);
		CHECK(counter.count == 0);
		CHECK(counter.points.size() == 2);
		CHECK(HookCounter::s_FieldHooks == 0);
		CHECK(HookCounter::s_ComponentHooks == 0);
		CHECK(changes.empty());
	}

	SUBCASE("A field without a hook runs only the component's")
	{
		HookCounter::ResetHooks();
		CHECK(Set(entity, Path("ReflectionTestHookCounter.Count"), 7, ChangeSource::Lua));
		CHECK(counter.count == 7);
		CHECK(HookCounter::s_FieldHooks == 0);
		CHECK(HookCounter::s_ComponentHooks == 1);
	}

	SUBCASE("Parts and list items")
	{
		changes.clear();
		CHECK(Set(entity, Path("ReflectionTestHookCounter.Points[1].Y"), 5.0f, ChangeSource::Sequence));
		CHECK(counter.points[1] == Vector2f(1.0f, 5.0f));
		REQUIRE(changes.size() == 1);
		CHECK(changes[0].oldValue == Value(2.0f));
		CHECK(changes[0].newValue == Value(5.0f));

		CHECK(Set(entity, Path("ReflectionTestHookCounter.Points[0]"), Vector2f(4.0f, 4.0f), ChangeSource::Lua));
		CHECK(counter.points[0] == Vector2f(4.0f, 4.0f));
		CHECK_FALSE(Set(entity, Path("ReflectionTestHookCounter.Points[2]"), Vector2f(4.0f, 4.0f), ChangeSource::Lua));

		CHECK(Set(entity, Path("ReflectionTestHookCounter.Points"), ValueList{ Vector2f(9.0f, 9.0f) }, ChangeSource::Lua));
		CHECK(counter.points == std::vector<Vector2f>{ Vector2f(9.0f, 9.0f) });
	}

	SUBCASE("Read-only fields can't be set")
	{
		counter.count = 4;
		CHECK(Get(entity, Path("ReflectionTestHookCounter.Total")) == Value(8));
		CHECK_FALSE(Set(entity, Path("ReflectionTestHookCounter.Total"), 2, ChangeSource::Lua));
		CHECK_FALSE(Set(entity, Path("ID.ID"), std::string("1-2"), ChangeSource::Lua));
		CHECK(entity.GetID() != Uuid(1, 2));
	}

	SUBCASE("A missing component can't be set")
	{
		CHECK_FALSE(Set(entity, Path("Sprite.TilingFactor"), 2.0f, ChangeSource::Lua));
	}

	scene.OnPropertyChanged().disconnect(listener);
}

TEST_CASE("Set checks enums, assets and layers fit the field")
{
	Setup();
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);

	PropertyPath shape = Path("Primitive.Type");
	CHECK(Set(root, shape, EnumOfValue(PrimitiveComponent::Shape::Cone), ChangeSource::Lua));
	CHECK(root.GetComponent<PrimitiveComponent>().type == PrimitiveComponent::Shape::Cone);
	CHECK_FALSE(Set(root, shape, EnumValue{ &EnumOf<PrimitiveComponent::Shape>(), 99 }, ChangeSource::Lua));
	CHECK_FALSE(Set(root, shape, EnumOfValue(BillboardComponent::Orientation::Camera), ChangeSource::Lua));
	CHECK(root.GetComponent<PrimitiveComponent>().type == PrimitiveComponent::Shape::Cone);

	Ref<Material> material = ComponentFixture::Asset<Material>("Materials/Other.material");
	CHECK_FALSE(Set(root, Path("Text.Font"), AssetValue{ material->GetFilepath(), material }, ChangeSource::Lua));
	CHECK(ComponentFixture::HasAsset(root.GetComponent<TextComponent>().font, "Fonts/Title.ttf"));
	CHECK(Set(root, Path("Primitive.Material"), AssetValue{ material->GetFilepath(), material }, ChangeSource::Lua));
	CHECK(root.GetComponent<PrimitiveComponent>().material == material);
	CHECK(Set(root, Path("Primitive.Material"), AssetPath("Materials/Third.material"), ChangeSource::Lua));
	CHECK(ComponentFixture::HasAsset(root.GetComponent<PrimitiveComponent>().material, "Materials/Third.material"));
	CHECK(Set(root, Path("Primitive.Material"), AssetValue{}, ChangeSource::Lua));
	CHECK(root.GetComponent<PrimitiveComponent>().material == nullptr);

	// A path is loaded, as there's no sprite atlas to draw it from
	CHECK(Set(root, Path("Sprite.Texture"), AssetPath("Textures/Other.png"), ChangeSource::Lua));
	CHECK(ComponentFixture::HasAsset(root.GetComponent<SpriteComponent>().texture, "Textures/Other.png"));

	CHECK(Set(root, Path("BoxCollider2D.Layer"), LayerValue{ 16 }, ChangeSource::Lua));
	CHECK(root.GetComponent<BoxCollider2DComponent>().layer == 16);
	CHECK_FALSE(Set(root, Path("BoxCollider2D.Layer"), LayerValue{ 3 }, ChangeSource::Lua));
	CHECK_FALSE(Set(root, Path("BoxCollider2D.Layer"), LayerValue{ 0 }, ChangeSource::Lua));
	CHECK_FALSE(Set(root, Path("BoxCollider2D.Layer"), MaskValue{ 1 }, ChangeSource::Lua));
	CHECK(Set(root, Path("BoxCollider2D.Mask"), MaskValue{ 0 }, ChangeSource::Lua));
	CHECK(root.GetComponent<BoxCollider2DComponent>().mask == 0);
}

TEST_CASE("Capture then Restore onto a default component reproduces the fixture")
{
	Setup();
	Scene source("");
	Entity sourceRoot = ComponentFixture::Create(source);

	Scene target("");
	Entity root = target.CreateEntity(*Uuid::Parse(ComponentFixture::k_RootId), "Root");
	Entity child = target.CreateEntity(*Uuid::Parse(ComponentFixture::k_ChildId), "Child");
	root.AddChild(child);
	// One restored onto a component that's already there, the rest added by Restore
	root.AddComponent<SpriteComponent>();

	const std::pair<Entity, Entity> pairs[] = { { sourceRoot, root }, { Child(sourceRoot), child } };
	for (const ComponentType* type : Components())
	{
		if (IsTestComponent(type) || type->IsInternal())
			continue;
		INFO(type->name);
		for (const auto& [from, to] : pairs)
		{
			if (!type->tryGet(source.GetRegistry(), from.GetHandle()))
				continue;
			ComponentValues values = Capture(from, *type);
			Restore(to, values, ChangeSource::Undo);

			ComponentValues restored = Capture(to, *type);
			REQUIRE(restored.values.size() == values.values.size());
			for (size_t i = 0; i < values.values.size(); ++i)
				CHECK_MESSAGE(restored.values[i] == values.values[i], (restored.values[i].ToString() + " should be " + values.values[i].ToString()));
			CHECK(restored.custom == values.custom);
		}
	}

	ComponentFixture::Check(root);
}

TEST_CASE("Restore runs each hook once and sends a signal per changed field")
{
	Setup();
	Scene scene("");
	Entity entity = scene.CreateEntity("Counter");
	scene.GetRegistry().emplace<HookCounter>(entity.GetHandle());

	ComponentValues values = Capture(entity, TypeOf<HookCounter>());
	REQUIRE(values.values.size() == 5);
	values.values[0] = 2.0f;	// Speed
	values.values[1] = 3.0f;	// Scale, which shares Speed's hook
	values.values[2] = 4;	// Count
	values.values[4] = 99;	// Total is read-only, so this is ignored

	std::vector<PropertyChanged> changes;
	ChangeRecorder listener{ &changes };
	scene.OnPropertyChanged().connect<&ChangeRecorder::OnChanged>(listener);

	HookCounter::ResetHooks();
	Restore(entity, values, ChangeSource::Undo);
	const HookCounter& counter = scene.GetRegistry().get<HookCounter>(entity.GetHandle());
	CHECK(counter.speed == 2.0f);
	CHECK(counter.scale == 3.0f);
	CHECK(counter.count == 4);
	CHECK(HookCounter::s_FieldHooks == 1);
	CHECK(HookCounter::s_ComponentHooks == 1);
	CHECK(changes.size() == 3);

	HookCounter::ResetHooks();
	Restore(entity, values, ChangeSource::Undo);
	CHECK(HookCounter::s_FieldHooks == 0);
	CHECK(HookCounter::s_ComponentHooks == 0);
	CHECK(changes.size() == 3);

	scene.OnPropertyChanged().disconnect(listener);
}

TEST_CASE("Defaults come from a default-constructed component")
{
	Setup();
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);

	for (const ComponentType* type : Components())
	{
		if (IsTestComponent(type) || type->IsInternal())
			continue;
		INFO(type->name);
		ComponentValues defaults = Defaults(*type);
		Entity holder = Holder(root, *type);
		Restore(holder, defaults, ChangeSource::Editor);
		ComponentValues reset = Capture(holder, *type);
		for (size_t i = 0; i < defaults.values.size(); ++i)
			CHECK_MESSAGE(reset.values[i] == defaults.values[i], (reset.values[i].ToString() + " should be " + defaults.values[i].ToString()));
	}

	CHECK(root.GetComponent<SpriteComponent>().tint == Colour(1.0f, 1.0f, 1.0f, 1.0f));
	CHECK(root.GetComponent<SpriteComponent>().texture == nullptr);
	CHECK(root.GetComponent<TilemapComponent>().tiles.empty());
	CHECK(root.GetComponent<StaticMeshComponent>().materialOverrides.empty());
	CHECK(Child(root).GetComponent<WeldJoint2DComponent>().stiffness == 0.0f);
}

TEST_CASE("Enum names read back, and unknown names fall back with a warning")
{
	Setup();

	for (const char* name : { "ProjectionType", "FilterMethod", "WrapMethod", "PrimitiveShape", "TilemapOrientation", "BodyType",
		"CapsuleDirection", "BillboardOrientation" })
	{
		INFO(name);
		CHECK(FindEnum(name) != nullptr);
	}

	for (const EnumType* type : Enums())
	{
		INFO(type->name);
		CHECK_FALSE(type->entries.empty());
		for (const EnumType::Entry& entry : type->entries)
		{
			CHECK(type->NameOf(entry.value) == entry.name);
			bool known = false;
			CHECK(EnumFromText(*type, entry.name, -1, &known) == entry.value);
			CHECK(known);
		}
	}

	const EnumType& bodyType = EnumOf<RigidBody2DComponent::BodyType>();
	CHECK(EnumFromText(bodyType, "Dynamic", 0) == (int32_t)RigidBody2DComponent::BodyType::DYNAMIC);
	bool known = true;
	CHECK(EnumFromText(bodyType, "Floaty", (int32_t)RigidBody2DComponent::BodyType::STATIC, &known) == (int32_t)RigidBody2DComponent::BodyType::STATIC);
	CHECK_FALSE(known);
	CHECK(bodyType.NameOf(42) == nullptr);
	CHECK(Value(EnumOfValue(RigidBody2DComponent::BodyType::KINEMATIC)).ToString() == "Kinematic");
	CHECK(EnumOf<TilemapComponent::Orientation>().NameOf((int32_t)TilemapComponent::Orientation::hexagonal) == std::string("Hexagonal"));
}

TEST_CASE("Layer and mask names read back, and unknown names fall back with a warning")
{
	CollisionLayers::Names names = CollisionLayers::DefaultNames();
	names[1] = "Player";
	names[3] = "Enemy";
	CollisionLayers::SetNames(names);

	CHECK(LayerToText(0x0001) == "Default");
	CHECK(LayerToText(0x0002) == "Player");
	CHECK(LayerToText(1 << 5) == "Layer 5");

	bool known = false;
	CHECK(LayerFromText("Player", &known) == 0x0002);
	CHECK(known);
	CHECK(LayerFromText(" Enemy ") == 0x0008);
	CHECK(LayerFromText("Layer 5", &known) == (1 << 5));
	CHECK(known);
	for (const char* unknown : { "Nope", "Layer 1", "Layer 16", "Layer x", "" })
	{
		INFO(unknown);
		CHECK(LayerFromText(unknown, &known) == CollisionLayers::DefaultLayer);
		CHECK_FALSE(known);
	}

	CHECK(MaskToText(0xFFFF) == "Everything");
	CHECK(MaskToText(0) == "Nothing");
	CHECK(MaskToText(0x0001 | 0x0002 | 0x0008) == "Default, Player, Enemy");
	CHECK(MaskToText(0x0001 | (1 << 5)) == "Default, Layer 5");

	for (uint16_t mask : { (uint16_t)0xFFFF, (uint16_t)0, (uint16_t)0x000B, (uint16_t)0x0021, (uint16_t)0x7FFF, (uint16_t)0x8000 })
	{
		INFO(mask);
		std::vector<std::string> unknown;
		CHECK(MaskFromText(MaskToText(mask), &unknown) == mask);
		CHECK(unknown.empty());
	}

	std::vector<std::string> unknown;
	CHECK(MaskFromText("Default, Nope, Enemy,Layer 1", &unknown) == (0x0001 | 0x0008));
	CHECK(unknown == std::vector<std::string>{ "Nope", "Layer 1" });

	CollisionLayers::SetNames(CollisionLayers::DefaultNames());
}

TEST_CASE("Setting each field of a primitive marks its mesh for rebuilding")
{
	Setup();
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);
	PrimitiveComponent& primitive = root.GetComponent<PrimitiveComponent>();
	const ComponentType& type = TypeOf<PrimitiveComponent>();

	for (int i = 0; i < (int)type.fields.size(); ++i)
	{
		const FieldInfo& field = type.fields[i];
		INFO(field.name);
		PropertyPath path{ &type, i };
		Value value = *Get(root, path);
		switch (field.type)
		{
		case ValueType::Float: value = value.As<float>() + 1.0f; break;
		case ValueType::UInt: value = value.As<uint32_t>() + 1; break;
		case ValueType::Enum: value = EnumOfValue(PrimitiveComponent::Shape::Sphere); break;
		case ValueType::Asset: value = AssetPath("Materials/Changed.material"); break;
		default: FAIL("No change for this field type"); break;
		}

		primitive.needsUpdating = false;
		REQUIRE(Set(root, path, value, ChangeSource::Editor));
		CHECK(primitive.needsUpdating);
	}

	// What the scene does before drawing it
	Ref<Mesh> before = primitive.mesh;
	primitive.SetType(primitive.type);
	CHECK(primitive.mesh != before);
	CHECK_FALSE(primitive.needsUpdating);
}

TEST_CASE("Setting each field of a tilemap rebuilds it")
{
	Setup();
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);
	TilemapComponent& tilemap = root.GetComponent<TilemapComponent>();
	const ComponentType& type = TypeOf<TilemapComponent>();

	auto rebuilds = [&tilemap]() { return tilemap.rebuildState ? tilemap.rebuildState->currentRebuildId : 0u; };

	for (int i = 0; i < (int)type.fields.size(); ++i)
	{
		const FieldInfo& field = type.fields[i];
		if (field.IsCustom())
			continue;
		INFO(field.name);
		PropertyPath path{ &type, i };
		Value value = *Get(root, path);
		switch (field.type)
		{
		case ValueType::Bool: value = !value.As<bool>(); break;
		case ValueType::UInt: value = value.As<uint32_t>() + 1; break;
		case ValueType::Colour: value = Colour(0.1f, 0.2f, 0.3f, 0.4f); break;
		case ValueType::Enum: value = EnumOfValue(TilemapComponent::Orientation::isometric); break;
		case ValueType::Asset: value = AssetPath("Tilesets/Changed.tileset"); break;
		case ValueType::Layer: value = LayerValue{ 8 }; break;
		case ValueType::Mask: value = MaskValue{ 3 }; break;
		default: FAIL("No change for this field type"); break;
		}

		uint32_t before = rebuilds();
		REQUIRE(Set(root, path, value, ChangeSource::Editor));
		CHECK(rebuilds() == before + 1);
	}

	// Width and height resize the grid, keeping the tiles that are left
	CHECK(tilemap.tilesWide == 4);
	CHECK(tilemap.tilesHigh == 3);
	CHECK(tilemap.tiles == std::vector<std::vector<uint32_t>>{ { 1, 2, 3, 0 }, { 4, 5, 6, 0 }, { 0, 0, 0, 0 } });

	REQUIRE(Set(root, Path("Tilemap.TilesWide"), 2u, ChangeSource::Editor));
	CHECK(tilemap.tiles == std::vector<std::vector<uint32_t>>{ { 1, 2 }, { 4, 5 }, { 0, 0 } });
}

TEST_CASE("Floats are written in the shortest form that reads back to the same bits")
{
	using SceneXml::FloatToText;
	CHECK(FloatToText(0.1f) == "0.1");
	CHECK(FloatToText(-9.81f) == "-9.81");
	CHECK(FloatToText(1.0f) == "1");
	CHECK(FloatToText(0.0f) == "0");
	CHECK(FloatToText(0.00999999978f) == "0.01");
	CHECK(FloatToText(FLT_MAX) == "3.4028235e+38");
	CHECK(FloatToText(std::numeric_limits<float>::denorm_min()) == "1.4013e-45");

	const float samples[] = { 0.1f, -9.81f, 1.0f / 3.0f, 0.78539819f, 123456.789f, 16777217.0f, 1e10f, -1e-10f, -0.0f,
		FLT_MIN, FLT_MAX, -FLT_MAX, FLT_EPSILON, std::numeric_limits<float>::denorm_min(), 1e-40f, 3.0e-39f };
	for (float value : samples)
	{
		const std::string text = FloatToText(value);
		INFO(text);
		const float read = std::strtof(text.c_str(), nullptr);
		CHECK(std::memcmp(&read, &value, sizeof(float)) == 0);
		CHECK(text.size() <= 15);
	}
}

namespace
{
	std::string RootNames(Scene& scene)
	{
		std::vector<std::string> names;
		for (Entity root : SceneGraph::GetRoots(scene))
		{
			std::string name = root.GetName();
			std::vector<Entity> children = SceneGraph::GetChildren(root);
			for (size_t i = 0; i < children.size(); ++i)
				name += (i == 0 ? "(" : " ") + children[i].GetName() + (i + 1 == children.size() ? ")" : "");
			names.push_back(name);
		}
		return Names(names);
	}
}

TEST_CASE("Roots keep their order through reordering, delete and undo, and save and load")
{
	Scene scene("");
	Entity a = scene.CreateEntity("A");
	Entity b = scene.CreateEntity("B");
	Entity c = scene.CreateEntity("C");
	Entity d = scene.CreateEntity("D");
	CHECK(RootNames(scene) == "A B C D");

	SceneGraph::MoveBefore(d, a);
	CHECK(RootNames(scene) == "D A B C");
	SceneGraph::MoveAfter(a, c);
	CHECK(RootNames(scene) == "D B C A");
	CHECK(scene.GetFirstRoot() == d.GetHandle());
	CHECK(scene.GetLastRoot() == a.GetHandle());

	SceneGraph::Reparent(b, c);
	CHECK(RootNames(scene) == "D C(B) A");
	SceneGraph::Unparent(b);
	CHECK(RootNames(scene) == "D C A B");

	SUBCASE("deleting a root and undoing puts it back in place")
	{
		RemoveEntityCommand remove(c);
		SceneGraph::Remove(c);
		CHECK(RootNames(scene) == "D A B");
		remove.Undo(scene);
		CHECK(RootNames(scene) == "D C A B");

		Entity last = scene.GetEntityByName("B");
		RemoveEntityCommand removeLast(last);
		SceneGraph::Remove(last);
		CHECK(scene.GetLastRoot() == scene.GetEntityByName("A").GetHandle());
		removeLast.Undo(scene);
		CHECK(RootNames(scene) == "D C A B");
	}

	SUBCASE("undoing a reparent puts a root back in place")
	{
		ReparentEntityCommand reparent(c, a);
		SceneGraph::Reparent(c, a);
		CHECK(RootNames(scene) == "D A(C) B");
		reparent.Undo(scene);
		CHECK(RootNames(scene) == "D C A B");
	}

	SUBCASE("saving and loading keeps the roots' and children's order")
	{
		Entity x = scene.CreateEntity("X");
		Entity y = scene.CreateEntity("Y");
		SceneGraph::Reparent(y, d);
		SceneGraph::Reparent(x, d);
		CHECK(RootNames(scene) == "D(Y X) C A B");

		const std::filesystem::path path = std::filesystem::temp_directory_path() / "AttercopRootOrder.scene";
		REQUIRE(SceneSerializer(&scene).Serialize(path));

		Scene loaded("");
		REQUIRE(SceneSerializer(&loaded).Deserialize(path));
		CHECK(RootNames(loaded) == "D(Y X) C A B");

		// A copy goes last
		loaded.DuplicateEntity(loaded.GetEntityByName("C"), Entity());
		CHECK(RootNames(loaded) == "D(Y X) C A B C");
		std::filesystem::remove(path);
	}
}

TEST_CASE("Renaming collision layers rewrites scenes so each collider keeps its layers")
{
	Setup();
	CollisionLayers::Names oldNames = CollisionLayers::DefaultNames();
	oldNames[1] = "Player";
	oldNames[2] = "Enemy";
	oldNames[3] = "Ground";

	// Player and Enemy swapped, Ground cleared, slot 5 named
	CollisionLayers::Names newNames = oldNames;
	newNames[1] = "Enemy";
	newNames[2] = "Player";
	newNames[3] = "";
	newNames[5] = "Water";

	tinyxml2::XMLDocument document;
	REQUIRE(document.Parse(R"(<Scene FormatVersion="2">
<Entity Name="A" ID="1-1"><BoxCollider2D Layer="Player" Mask="Default, Enemy, Ground, Layer 5"/>
<Entity Name="B" ID="1-2"><Tilemap Layer="Ground" Mask="Everything"/><CircleCollider2D Layer="Typo" Mask="Nothing"/></Entity>
</Entity></Scene>)") == tinyxml2::XML_SUCCESS);

	CHECK(SceneXml::RenameCollisionLayers(document.RootElement(), oldNames, newNames));
	const tinyxml2::XMLElement* a = document.RootElement()->FirstChildElement("Entity");
	CHECK(std::string(a->FirstChildElement("BoxCollider2D")->Attribute("Layer")) == "Enemy");
	CHECK(std::string(a->FirstChildElement("BoxCollider2D")->Attribute("Mask")) == "Default, Player, Layer 3, Water");
	const tinyxml2::XMLElement* b = a->FirstChildElement("Entity");
	CHECK(std::string(b->FirstChildElement("Tilemap")->Attribute("Layer")) == "Layer 3");
	CHECK(std::string(b->FirstChildElement("Tilemap")->Attribute("Mask")) == "Everything");
	CHECK(std::string(b->FirstChildElement("CircleCollider2D")->Attribute("Layer")) == "Typo");

	// Read with the new names, the bits are the ones the old names gave
	CollisionLayers::SetNames(newNames);
	CHECK(LayerFromText(a->FirstChildElement("BoxCollider2D")->Attribute("Layer")) == (1 << 1));
	CHECK(MaskFromText(a->FirstChildElement("BoxCollider2D")->Attribute("Mask")) == (1 | (1 << 2) | (1 << 3) | (1 << 5)));
	CollisionLayers::SetNames(CollisionLayers::DefaultNames());

	CHECK_FALSE(SceneXml::RenameCollisionLayers(document.RootElement(), newNames, newNames));
}

TEST_CASE("Every described field has an inspector widget for its value type")
{
	Setup();
	// Drawn by the Editor's overrides
	const std::set<std::string> overridden = { "StaticMesh.Materials", "Tilemap.Tiles" };
	for (const ComponentType* type : Components())
	{
		if (IsTestComponent(type) || type->IsInternal())
			continue;
		for (const FieldInfo& field : type->fields)
		{
			if (field.Has(FieldFlags::HideInInspector))
				continue;
			const std::string path = type->name + "." + field.name;
			INFO(path);
			if (field.IsCustom())
				CHECK(overridden.count(path) == 1);
			else
				CHECK(Inspector::WidgetFor(field) != Inspector::Widget::None);
		}
	}
}

TEST_CASE("An inspector drag's changes share an interaction number, and other changes have 0")
{
	Setup();
	Scene scene("");
	Entity entity = scene.CreateEntity("Mover");
	entity.AddComponent<TransformComponent>();

	std::vector<PropertyChanged> changes;
	ChangeRecorder listener{ &changes };
	scene.OnPropertyChanged().connect<&ChangeRecorder::OnChanged>(listener);

	PropertyEdits edits;
	const PropertyPath position = Path("Transform.Position");
	edits.Begin(entity, position);
	for (float x : { 1.0f, 2.0f, 3.0f })
		REQUIRE(edits.Set(entity, position, Vector3f(x, 0.0f, 0.0f)));
	edits.End();

	REQUIRE(changes.size() == 3);
	CHECK(changes[0].interaction != 0);
	CHECK(changes[1].interaction == changes[0].interaction);
	CHECK(changes[2].interaction == changes[0].interaction);

	// The next drag, a one-off inspector change and a change from elsewhere
	edits.Begin(entity, position);
	REQUIRE(edits.Set(entity, position, Vector3f(4.0f, 0.0f, 0.0f)));
	edits.End();
	REQUIRE(edits.Set(entity, Path("Transform.Scale"), Vector3f(2.0f, 2.0f, 2.0f)));
	REQUIRE(Set(entity, position, Vector3f(5.0f, 0.0f, 0.0f), ChangeSource::Lua));

	REQUIRE(changes.size() == 6);
	CHECK(changes[3].interaction != 0);
	CHECK(changes[3].interaction != changes[0].interaction);
	CHECK(changes[4].interaction != 0);
	CHECK(changes[4].interaction != changes[3].interaction);
	CHECK(changes[5].interaction == 0);
	for (size_t i = 0; i < 5; ++i)
		CHECK(changes[i].source == ChangeSource::Editor);

	scene.OnPropertyChanged().disconnect(listener);
}

TEST_CASE("A resize applied from the captured start keeps the tiles")
{
	Setup();
	Scene scene("");
	Entity root = ComponentFixture::Create(scene);
	TilemapComponent& tilemap = root.GetComponent<TilemapComponent>();
	REQUIRE(tilemap.tiles == std::vector<std::vector<uint32_t>>{ { 1, 2, 3 }, { 4, 5, 6 } });

	// Typing 100 over 3: the width is 1, then 10, then 100
	PropertyEdits edits;
	const PropertyPath width = Path("Tilemap.TilesWide");
	edits.Begin(root, width);
	for (uint32_t typed : { 1u, 10u, 100u })
		REQUIRE(edits.Set(root, width, typed));
	edits.End();

	CHECK(tilemap.tilesWide == 100);
	REQUIRE(tilemap.tiles.size() == 2);
	REQUIRE(tilemap.tiles[0].size() == 100);
	CHECK(std::vector<uint32_t>(tilemap.tiles[0].begin(), tilemap.tiles[0].begin() + 4) == std::vector<uint32_t>{ 1, 2, 3, 0 });
	CHECK(std::vector<uint32_t>(tilemap.tiles[1].begin(), tilemap.tiles[1].begin() + 4) == std::vector<uint32_t>{ 4, 5, 6, 0 });

	// Each step set on its own loses the cut-off tiles
	REQUIRE(Set(root, width, 1u, ChangeSource::Editor));
	REQUIRE(Set(root, width, 3u, ChangeSource::Editor));
	CHECK(tilemap.tiles == std::vector<std::vector<uint32_t>>{ { 1, 0, 0 }, { 4, 0, 0 } });
}
TEST_CASE("A component that excludes another names it, either way round")
{
	RegisterComponents();
	for (const ComponentType* type : Components())
	{
		for (const std::string& name : type->excludes)
			CHECK_MESSAGE(FindComponent(name) != nullptr, (type->name + " excludes " + name));
	}

	Scene scene("");
	Entity entity = scene.CreateEntity("Entity");
	entity.AddComponent<TransformComponent>();
	CHECK(ExcludedBy(scene.GetRegistry(), entity.GetHandle(), TypeOf<WidgetComponent>()) == &TypeOf<TransformComponent>());
	CHECK(ExcludedBy(scene.GetRegistry(), entity.GetHandle(), TypeOf<SpriteComponent>()) == nullptr);

	Entity widget = scene.CreateEntity("Widget");
	widget.AddComponent<WidgetComponent>();
	CHECK(ExcludedBy(scene.GetRegistry(), widget.GetHandle(), TypeOf<TransformComponent>()) == &TypeOf<WidgetComponent>());
}
}
