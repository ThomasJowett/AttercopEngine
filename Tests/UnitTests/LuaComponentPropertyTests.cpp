#include "vendor/doctest.h"
#include "ComponentFixture.h"

#include "Reflection/Reflection.h"
#include "Scripting/Lua/LuaManager.h"

#include <functional>
#include <map>
#include <set>
#include <string>

// The Lua properties of each component, as scripts and auto-complete see them today

namespace
{
	// Component, then "Property:LuaType" sorted by name
	const std::map<std::string, std::string> k_LuaProperties = {
		{ "AnimatedSpriteComponent", "Animation:string SpriteSheet:SpriteSheet Tint:Colour" },
		{ "AudioListenerComponent", "Primary:boolean" },
		{ "AudioSourceComponent", "Clip:AudioClip Loop:boolean MaxDistance:number MinDistance:number Pitch:number PlayOnStart:boolean Rolloff:number Stream:boolean Volume:number" },
		{ "BehaviourTreeComponent", "Filepath:string" },
		{ "BillboardComponent", "Orientation:BillboardOrientation" },
		{ "BoxCollider2DComponent", "IsTrigger:boolean Layer:string Mask:table of string Offset:Vec2 PhysicsMaterial:PhysicsMaterial Size:Vec2" },
		{ "ButtonComponent", "ClickedFilterMethod:FilterMethod ClickedTexture:Texture2D ClickedTint:Colour ClickedWrapMethod:WrapMethod DisabledFilterMethod:FilterMethod DisabledTexture:Texture2D DisabledTint:Colour DisabledWrapMethod:WrapMethod HoveredFilterMethod:FilterMethod HoveredTexture:Texture2D HoveredTint:Colour HoveredWrapMethod:WrapMethod NormalFilterMethod:FilterMethod NormalTexture:Texture2D NormalTint:Colour NormalWrapMethod:WrapMethod" },
		{ "CameraComponent", "AspectRatio:number Camera:Camera FixedAspectRatio:boolean Fov:number OrthoFar:number OrthoNear:number OrthoSize:number PerspectiveFar:number PerspectiveNear:number Primary:boolean Projection:ProjectionType" },
		{ "CanvasComponent", "PixelPerUnit:number" },
		{ "CapsuleCollider2DComponent", "Direction:CapsuleDirection Height:number IsTrigger:boolean Layer:string Mask:table of string Offset:Vec2 PhysicsMaterial:PhysicsMaterial Radius:number" },
		{ "CircleCollider2DComponent", "IsTrigger:boolean Layer:string Mask:table of string Offset:Vec2 PhysicsMaterial:PhysicsMaterial Radius:number" },
		{ "CircleRendererComponent", "Colour:Colour Fade:number Radius:number Thickness:number" },
		{ "GridLayoutComponent", "CellSpacing:Vec2 Columns:integer FixedRowHeight:number PaddingBottom:number PaddingLeft:number PaddingRight:number PaddingTop:number UniformCellSize:boolean" },
		{ "HiddenComponent", "Hidden:boolean" },
		{ "LuaScriptComponent", "Script:LuaScript" },
		{ "PointLightComponent", "Attenuation:number CastShadows:boolean Colour:Colour Range:number" },
		{ "PolygonCollider2DComponent", "IsTrigger:boolean Layer:string Mask:table of string Offset:Vec2 PhysicsMaterial:PhysicsMaterial Vertices:table of Vec2" },
		{ "PrimitiveComponent", "ConeBottomRadius:number ConeHeight:number ConeSliceCount:integer ConeStackCount:integer CubeDepth:number CubeHeight:number CubeWidth:number CylinderBottomRadius:number CylinderHeight:number CylinderSliceCount:integer CylinderStackCount:integer CylinderTopRadius:number Material:Material Mesh:Mesh PlaneLength:number PlaneLengthLines:integer PlaneTileU:number PlaneTileV:number PlaneWidth:number PlaneWidthLines:integer SphereLatitudeLines:integer SphereLongitudeLines:integer SphereRadius:number TorusInnerRadius:number TorusOuterRadius:number TorusSliceCount:integer Type:PrimitiveShape" },
		{ "RigidBody2DComponent", "AngularDamping:number FixedRotation:boolean GravityScale:number LinearDamping:number Type:BodyType" },
		{ "ScrollBoxComponent", "ClipContent:boolean HorizontalScroll:boolean ScrollOffset:Vec2 VerticalScroll:boolean" },
		{ "SpriteComponent", "FilterMethod:FilterMethod Texture:Texture2D TilingFactor:number Tint:Colour WrapMethod:WrapMethod" },
		{ "StackLayoutComponent", "Horizontal:boolean PaddingBottom:number PaddingLeft:number PaddingRight:number PaddingTop:number Spacing:number StretchCrossAxis:boolean" },
		{ "StateMachineComponent", "Filepath:string" },
		{ "StaticMeshComponent", "Mesh:StaticMesh" },
		{ "TextComponent", "Colour:Colour Font:Font MaxWidth:number Text:string" },
		{ "TilemapComponent", "IsTrigger:boolean Layer:string Mask:table of string Orientation:TilemapOrientation TileHeight:integer TileWidth:integer TilesHigh:integer TilesWide:integer Tileset:Tileset Tint:Colour" },
		{ "TransformComponent", "Orientation:Quaternion Position:Vec3 Rotation:Vec3 Scale:Vec3" },
		{ "WeldJoint2DComponent", "CollideConnected:boolean ConnectedEntity:Entity Damping:number Stiffness:number" },
		{ "WidgetComponent", "AnchorBottom:number AnchorLeft:number AnchorRight:number AnchorTop:number Disabled:boolean FixedHeight:boolean FixedWidth:boolean MarginBottom:number MarginLeft:number MarginRight:number MarginTop:number Position:Vec2 Rotation:number Size:Vec2" },
	};

	// The saved form of a component, by its Lua name; empty if no component has that name
	std::string SavedForm(const std::string& luaName, Entity entity)
	{
		RegisterComponents();
		for (const Reflection::ComponentType* type : Reflection::Components())
		{
			if (type->luaName != luaName)
				continue;
			const Reflection::ComponentValues values = Reflection::Capture(entity, *type);
			std::string text = values.custom;
			for (const Reflection::Value& value : values.values)
				text += "|" + value.ToString();
			return text;
		}
		return {};
	}

	struct LuaFixture
	{
		Scene scene{ "" };
		Entity root;

		LuaFixture()
		{
			LuaManager::Init();
			root = ComponentFixture::Create(scene);
			LuaManager::GetState()["root"] = root;
			LuaManager::GetState()["child"] = root.GetChild();
		}
		~LuaFixture() { LuaManager::Shutdown(); }

		// Runs a chunk of Lua, reporting its error
		bool Run(const std::string& code)
		{
			sol::protected_function_result result = LuaManager::GetState().safe_script(code, sol::script_pass_on_error);
			if (!result.valid())
			{
				sol::error error = result;
				FAIL_CHECK(error.what());
			}
			return result.valid();
		}
	};
}

TEST_CASE("Each component has the same Lua properties")
{
	LuaManager::Init();
	std::map<std::string, std::set<std::string>> found;
	for (const LuaApiEntry& entry : LuaManager::GetIdentifiers())
		// Test-only components, named with their namespace, are left out
		if (entry.kind == LuaApiEntry::Kind::Property && entry.isComponent && entry.component.find("::") == std::string::npos)
			found[entry.component].insert(entry.name + ":" + entry.type);
	LuaManager::Shutdown();

	std::map<std::string, std::string> actual;
	for (const auto& [component, properties] : found)
	{
		std::string joined;
		for (const std::string& property : properties)
			joined += (joined.empty() ? "" : " ") + property;
		actual[component] = joined;
	}

	for (const auto& [component, properties] : k_LuaProperties)
	{
		const auto& componentName = component;
		CAPTURE(componentName);
		CHECK(actual[component] == properties);
	}
	CHECK(actual.size() == k_LuaProperties.size());
}

TEST_CASE("Assigning a Lua property its own value leaves the component unchanged")
{
	LuaFixture lua;
	std::set<std::string> readOnly;
	for (const LuaApiEntry& entry : LuaManager::GetIdentifiers())
	{
		if (entry.kind != LuaApiEntry::Kind::Property || !entry.isComponent || entry.component.find("::") != std::string::npos)
			continue;
		CAPTURE(entry.component);
		CAPTURE(entry.name);
		Entity owner = entry.component == "WeldJoint2DComponent" ? lua.root.GetChild() : lua.root;
		const char* ownerName = entry.component == "WeldJoint2DComponent" ? "child" : "root";
		std::string before = SavedForm(entry.component, owner);
		REQUIRE_FALSE(before.empty());

		std::string component = std::string(ownerName) + ":Get" + entry.component + "()";
		sol::protected_function_result result = LuaManager::GetState().safe_script(
			"local c = " + component + "\nc." + entry.name + " = c." + entry.name, sol::script_pass_on_error);
		if (!result.valid())
		{
			sol::error error = result;
			if (std::string(error.what()).find("is read-only") != std::string::npos)
				readOnly.insert(entry.component + "." + entry.name);
			else
				FAIL_CHECK(error.what());
		}
		CHECK(SavedForm(entry.component, owner) == before);
	}

	// Every bound field can be set
	CHECK(readOnly.empty());
}

TEST_CASE("Values set from Lua reach the component")
{
	LuaFixture lua;
	REQUIRE(lua.Run(R"(
		local transform = root:GetTransformComponent()
		transform.Position = Vec3.new(9, 8, 7)
		root:GetSpriteComponent().Tint = Colour.new(0.25, 0.5, 1, 1)
		root:GetSpriteComponent().FilterMethod = FilterMethod.Nearest
		local body = root:GetRigidBody2DComponent()
		body.Type = BodyType.Kinematic
		body.FixedRotation = false
		body.GravityScale = 2
		root:GetTextComponent().Text = "From Lua"
		root:GetBoxCollider2DComponent().Size = Vec2.new(3, 4)
		root:GetCircleCollider2DComponent().Radius = 6
		root:GetTilemapComponent().TileWidth = 48
		root:GetPrimitiveComponent().TorusSliceCount = 7
		root:GetHiddenComponent().Hidden = true
		root:SetName("Renamed")
		child:GetWeldJoint2DComponent().Stiffness = 9
		root:GetBoxCollider2DComponent().IsTrigger = true
		root:GetBoxCollider2DComponent().Layer = "Layer 4"
		root:GetBoxCollider2DComponent().Mask = { "Default", "Layer 5" }
		root:GetCameraComponent().OrthoSize = 30
		root:GetCameraComponent().Projection = ProjectionType.Perspective
		root:GetAudioSourceComponent().PlayOnStart = false
		root:GetPointLightComponent().Range = 12
		root:GetPrimitiveComponent().Type = PrimitiveShape.Sphere
	)"));

	CHECK(lua.root.GetComponent<TransformComponent>().GetPosition() == Vector3f(9.0f, 8.0f, 7.0f));
	CHECK(lua.root.GetComponent<SpriteComponent>().tint == Colour(0.25f, 0.5f, 1.0f, 1.0f));
	CHECK(lua.root.GetComponent<SpriteComponent>().filterMethod == Texture::FilterMethod::Nearest);
	const RigidBody2DComponent& body = lua.root.GetComponent<RigidBody2DComponent>();
	CHECK(body.type == RigidBody2DComponent::BodyType::KINEMATIC);
	CHECK_FALSE(body.fixedRotation);
	CHECK(body.gravityScale == 2.0f);
	CHECK(lua.root.GetComponent<TextComponent>().text == "From Lua");
	CHECK(lua.root.GetComponent<BoxCollider2DComponent>().size == Vector2f(3.0f, 4.0f));
	CHECK(lua.root.GetComponent<CircleCollider2DComponent>().radius == 6.0f);
	CHECK(lua.root.GetComponent<TilemapComponent>().tileWidth == 48);
	CHECK(lua.root.GetComponent<PrimitiveComponent>().torusSliceCount == 7);
	CHECK(lua.root.GetComponent<HiddenComponent>().hidden);
	CHECK(lua.root.GetName() == "Renamed");
	CHECK(lua.root.GetChild().GetComponent<WeldJoint2DComponent>().stiffness == 9.0f);
	const BoxCollider2DComponent& box = lua.root.GetComponent<BoxCollider2DComponent>();
	CHECK(box.isTrigger);
	CHECK(box.layer == (1 << 4));
	CHECK(box.mask == (1 | (1 << 5)));
	const CameraComponent& camera = lua.root.GetComponent<CameraComponent>();
	CHECK(camera.camera.GetOrthoSize() == 30.0f);
	CHECK(camera.camera.GetProjectionType() == SceneCamera::ProjectionType::perspective);
	CHECK_FALSE(lua.root.GetComponent<AudioSourceComponent>().playOnStart);
	CHECK(lua.root.GetComponent<PointLightComponent>().range == 12.0f);
	CHECK(lua.root.GetComponent<PrimitiveComponent>().type == PrimitiveComponent::Shape::Sphere);
}

TEST_CASE("Values set in C++ read back in Lua")
{
	LuaFixture lua;
	REQUIRE(lua.Run(R"(
		local function check(condition, message) if not condition then error(message, 2) end end
		local position = root:GetTransformComponent().Position
		check(position.x == 1 and position.y == 2 and position.z == 3, "Transform.Position")
		local tint = root:GetSpriteComponent().Tint
		check(tint.r == 0.5 and tint.g == 0.25 and tint.b == 0.75 and tint.a == 0.5, "Sprite.Tint")
		check(root:GetSpriteComponent().WrapMethod == WrapMethod.Mirror, "Sprite.WrapMethod")
		check(root:GetRigidBody2DComponent().Type == BodyType.Dynamic, "RigidBody2D.Type")
		check(root:GetTextComponent().Text == "Hello, world", "Text.Text")
		check(root:GetAnimatedSpriteComponent().Animation == "Run", "AnimatedSprite.Animation")
		check(#root:GetPolygonCollider2DComponent().Vertices == 4, "PolygonCollider2D.Vertices")
		check(root:GetAudioSourceComponent().Loop == true, "AudioSource.Loop")
		check(root:GetWidgetComponent().Size.x == 160, "Widget.Size")
		check(root:GetCameraComponent().Primary == false, "Camera.Primary")
		check(child:GetWeldJoint2DComponent().CollideConnected == true, "WeldJoint2D.CollideConnected")
	)"));
}

namespace
{
	// Records each PropertyChanged the scene sends
	struct ChangeLog
	{
		std::vector<Reflection::PropertyChanged> changes;
		void OnChanged(const Reflection::PropertyChanged& change) { changes.push_back(change); }
	};
}

TEST_CASE("A Lua setter runs the same OnChanged hooks as Reflection::Set")
{
	LuaFixture lua;
	ChangeLog log;
	lua.scene.OnPropertyChanged().connect<&ChangeLog::OnChanged>(log);

	PrimitiveComponent& primitive = lua.root.GetComponent<PrimitiveComponent>();
	primitive.needsUpdating = false;
	REQUIRE(lua.Run("root:GetPrimitiveComponent().TorusOuterRadius = 4"));
	CHECK(primitive.torusOuterRadius == 4.0f);
	CHECK(primitive.needsUpdating);

	primitive.needsUpdating = false;
	REQUIRE(Reflection::Set(lua.root, *Reflection::PropertyPath::Parse("Primitive.TorusOuterRadius"), 5.0f, Reflection::ChangeSource::Editor));
	CHECK(primitive.needsUpdating);

	REQUIRE(lua.Run("root:GetTilemapComponent().TilesWide = 6"));
	const TilemapComponent& tilemap = lua.root.GetComponent<TilemapComponent>();
	REQUIRE_FALSE(tilemap.tiles.empty());
	CHECK(tilemap.tiles[0].size() == 6);

	REQUIRE(log.changes.size() == 3);
	CHECK(log.changes[0].source == Reflection::ChangeSource::Lua);
	CHECK(log.changes[0].path.ToString() == "Primitive.TorusOuterRadius");
	CHECK(log.changes[1].source == Reflection::ChangeSource::Editor);
	CHECK(log.changes[2].source == Reflection::ChangeSource::Lua);
	CHECK(log.changes[2].path.ToString() == "Tilemap.TilesWide");
	lua.scene.OnPropertyChanged().disconnect(log);
}

TEST_CASE("Lua errors name the property for wrong types and unknown layers")
{
	LuaFixture lua;
	auto error = [&lua](const std::string& code)
		{
			sol::protected_function_result result = LuaManager::GetState().safe_script(code, sol::script_pass_on_error);
			if (result.valid())
				return std::string();
			sol::error failure = result;
			return std::string(failure.what());
		};
	CHECK(error("root:GetBoxCollider2DComponent().Size = 3").find("BoxCollider2DComponent.Size can't be set to a number") != std::string::npos);
	CHECK(error("root:GetBoxCollider2DComponent().Layer = \"Nowhere\"").find("no collision layer called \"Nowhere\"") != std::string::npos);
	CHECK(error("root:GetSpriteComponent().Sparkle = 1").find("SpriteComponent.Sparkle isn't a property") != std::string::npos);
}

TEST_CASE("Lua can't add a component beside one it excludes")
{
	LuaFixture lua;
	for (const char* add : { "child:AddWidgetComponent()", "child:GetOrAddWidgetComponent()" })
	{
		CAPTURE(add);
		sol::protected_function_result result = LuaManager::GetState().safe_script(add, sol::script_pass_on_error);
		REQUIRE_FALSE(result.valid());
		sol::error failure = result;
		CHECK(std::string(failure.what()).find("Child has a TransformComponent, which can't be on the same entity as a WidgetComponent") != std::string::npos);
	}
	CHECK_FALSE(lua.root.GetChild().HasComponent<WidgetComponent>());
}
