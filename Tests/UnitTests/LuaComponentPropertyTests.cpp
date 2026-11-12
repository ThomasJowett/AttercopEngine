#include "vendor/doctest.h"
#include "ComponentFixture.h"

#include "Scripting/Lua/LuaManager.h"

#include "cereal/archives/binary.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"

#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>

// The Lua properties of each component, as scripts and auto-complete see them today

namespace
{
	// Component, then "Property:LuaType" sorted by name
	const std::map<std::string, std::string> k_LuaProperties = {
		{ "AnimatedSpriteComponent", "Animation:string SpriteSheet:SpriteSheet Tint:Colour" },
		{ "AudioSourceComponent", "Clip:AudioClip Loop:boolean MaxDistance:number MinDistance:number Pitch:number Rolloff:number Stream:boolean Volume:number" },
		{ "BoxCollider2DComponent", "Layer:string Mask:table of string Offset:Vec2 PhysicsMaterial:PhysicsMaterial Size:Vec2" },
		{ "ButtonComponent", "ClickedTint:Colour DisabledTint:Colour HoveredTint:Colour NormalTint:Colour" },
		{ "CameraComponent", "Camera:Camera FixedAspectRatio:boolean Primary:boolean" },
		{ "CanvasComponent", "PixelPerUnit:number" },
		{ "CapsuleCollider2DComponent", "Height:number Layer:string Mask:table of string Offset:Vec2 PhysicsMaterial:PhysicsMaterial Radius:number" },
		{ "CircleCollider2DComponent", "Layer:string Mask:table of string Offset:Vec2 PhysicsMaterial:PhysicsMaterial Radius:number" },
		{ "CircleRendererComponent", "Colour:Colour Fade:number Radius:number Thickness:number" },
		{ "HiddenComponent", "Hidden:boolean" },
		{ "NameComponent", "Name:string" },
		{ "PolygonCollider2DComponent", "Layer:string Mask:table of string Offset:Vec2 PhysicsMaterial:PhysicsMaterial Vertices:table of Vec2" },
		{ "PrimitiveComponent", "ConeBottomRadius:number ConeHeight:number ConeSliceCount:integer ConeStackCount:integer CubeDepth:number "
			"CubeHeight:number CubeWidth:number CylinderBottomRadius:number CylinderHeight:number CylinderSliceCount:integer "
			"CylinderStackCount:integer CylinderTopRadius:number Material:Material Mesh:Mesh PlaneLength:number PlaneLengthLines:integer "
			"PlaneTileU:number PlaneTileV:number PlaneWidth:number PlaneWidthLines:integer SphereLatitudeLines:integer "
			"SphereLongitudeLines:integer SphereRadius:number TorusInnerRadius:number TorusOuterRadius:number TorusSliceCount:integer "
			"Type:integer (enum)" },
		{ "RigidBody2DComponent", "AngularDamping:number FixedRotation:boolean GravityScale:number LinearDamping:number Type:BodyType" },
		{ "SpriteComponent", "FilterMethod:FilterMethod Texture:Texture2D TilingFactor:number Tint:Colour WrapMethod:WrapMethod" },
		{ "TextComponent", "Colour:Colour Font:Font MaxWidth:number Text:string" },
		{ "TilemapComponent", "IsTrigger:boolean Layer:string Mask:table of string Orientation:integer (enum) TileHeight:integer "
			"TileWidth:integer TilesHigh:integer TilesWide:integer Tileset:Tileset Tint:Colour" },
		{ "TransformComponent", "Position:Vec3 Rotation:Vec3 Scale:Vec3" },
		{ "WeldJoint2DComponent", "CollideConnected:boolean Damping:number Stiffness:number" },
		{ "WidgetComponent", "Position:Vec2 Size:Vec2" },
	};

	template<typename T>
	std::function<std::string(Entity)> SnapshotBytes()
	{
		return [](Entity entity)
			{
				std::stringstream stream;
				{
					cereal::BinaryOutputArchive output(stream);
					output(entity.GetComponent<T>());
				}
				return stream.str();
			};
	}

	// The saved form of a component, by name
	const std::map<std::string, std::function<std::string(Entity)>> k_Snapshots = {
		{ "AnimatedSpriteComponent", SnapshotBytes<AnimatedSpriteComponent>() },
		{ "AudioSourceComponent", SnapshotBytes<AudioSourceComponent>() },
		{ "BoxCollider2DComponent", SnapshotBytes<BoxCollider2DComponent>() },
		{ "ButtonComponent", SnapshotBytes<ButtonComponent>() },
		{ "CameraComponent", SnapshotBytes<CameraComponent>() },
		{ "CanvasComponent", SnapshotBytes<CanvasComponent>() },
		{ "CapsuleCollider2DComponent", SnapshotBytes<CapsuleCollider2DComponent>() },
		{ "CircleCollider2DComponent", SnapshotBytes<CircleCollider2DComponent>() },
		{ "CircleRendererComponent", SnapshotBytes<CircleRendererComponent>() },
		{ "HiddenComponent", SnapshotBytes<HiddenComponent>() },
		{ "NameComponent", SnapshotBytes<NameComponent>() },
		{ "PolygonCollider2DComponent", SnapshotBytes<PolygonCollider2DComponent>() },
		{ "PrimitiveComponent", SnapshotBytes<PrimitiveComponent>() },
		{ "RigidBody2DComponent", SnapshotBytes<RigidBody2DComponent>() },
		{ "SpriteComponent", SnapshotBytes<SpriteComponent>() },
		{ "TextComponent", SnapshotBytes<TextComponent>() },
		{ "TilemapComponent", SnapshotBytes<TilemapComponent>() },
		{ "TransformComponent", SnapshotBytes<TransformComponent>() },
		{ "WeldJoint2DComponent", SnapshotBytes<WeldJoint2DComponent>() },
		{ "WidgetComponent", SnapshotBytes<WidgetComponent>() },
	};

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
		if (entry.kind == LuaApiEntry::Kind::Property && entry.isComponent)
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
		CAPTURE(component);
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
		if (entry.kind != LuaApiEntry::Kind::Property || !entry.isComponent)
			continue;
		CAPTURE(entry.component);
		CAPTURE(entry.name);
		REQUIRE(k_Snapshots.count(entry.component) == 1);

		Entity owner = entry.component == "WeldJoint2DComponent" ? lua.root.GetChild() : lua.root;
		const char* ownerName = entry.component == "WeldJoint2DComponent" ? "child" : "root";
		std::string before = k_Snapshots.at(entry.component)(owner);

		std::string component = std::string(ownerName) + ":Get" + entry.component + "()";
		sol::protected_function_result result = LuaManager::GetState().safe_script(
			"local c = " + component + "\nc." + entry.name + " = c." + entry.name, sol::script_pass_on_error);
		if (!result.valid())
		{
			sol::error error = result;
			if (std::string(error.what()).find("readonly property") != std::string::npos)
				readOnly.insert(entry.component + "." + entry.name);
			else
				FAIL_CHECK(error.what());
		}
		CHECK(k_Snapshots.at(entry.component)(owner) == before);
	}

	// Collision filters can be read but not set from Lua
	const std::set<std::string> expectedReadOnly = {
		"BoxCollider2DComponent.Layer", "BoxCollider2DComponent.Mask",
		"CapsuleCollider2DComponent.Layer", "CapsuleCollider2DComponent.Mask",
		"CircleCollider2DComponent.Layer", "CircleCollider2DComponent.Mask",
		"PolygonCollider2DComponent.Layer", "PolygonCollider2DComponent.Mask",
		"TilemapComponent.Layer", "TilemapComponent.Mask",
	};
	CHECK(readOnly == expectedReadOnly);
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
		root:GetNameComponent().Name = "Renamed"
		child:GetWeldJoint2DComponent().Stiffness = 9
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
