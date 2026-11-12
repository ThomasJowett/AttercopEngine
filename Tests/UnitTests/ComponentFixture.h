#pragma once

#include "vendor/doctest.h"

#include "Scene/AssetManager.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <filesystem>
#include <string>

// An entity with every component a scene saves, each with values that differ from its defaults, and checks for
// each value. Asset files don't exist, so assets load as placeholders that keep their path.
namespace ComponentFixture
{
	inline const char* k_RootId = "1001-2002";
	inline const char* k_ChildId = "3003-4004";

	inline Ref<Texture2D> Texture(const char* path) { return AssetManager::GetTexture(path); }

	template<typename T>
	Ref<T> Asset(const char* path) { return AssetManager::GetAsset<T>(path); }

	inline bool SamePath(const std::filesystem::path& actual, const char* expected)
	{
		return actual == std::filesystem::path(expected).make_preferred();
	}

	template<typename T>
	bool HasAsset(const Ref<T>& asset, const char* path) { return asset && SamePath(asset->GetFilepath(), path); }

	inline Entity Create(Scene& scene)
	{
		Entity root = scene.CreateEntity(Uuid(std::string(k_RootId)), "Root");
		root.AddComponent<TransformComponent>(Vector3f(1.0f, 2.0f, 3.0f), Vector3f(0.25f, 0.5f, 0.75f), Vector3f(2.0f, 3.0f, 4.0f));

		CameraComponent& camera = root.AddComponent<CameraComponent>();
		camera.primary = false;
		camera.fixedAspectRatio = true;
		camera.camera.SetOrthoSize(12.0f);
		camera.camera.SetOrthoNear(-2.0f);
		camera.camera.SetOrthoFar(3.0f);
		camera.camera.SetPerspectiveNear(0.5f);
		camera.camera.SetPerspectiveFar(250.0f);
		camera.camera.SetVerticalFov(0.75f);
		camera.camera.SetAspectRatio(2.0f);
		camera.camera.SetProjection(SceneCamera::ProjectionType::orthographic);

		SpriteComponent& sprite = root.AddComponent<SpriteComponent>();
		sprite.tint = Colour(0.5f, 0.25f, 0.75f, 0.5f);
		sprite.tilingFactor = 3.0f;
		sprite.texture = Texture("Textures/Brick.png");
		sprite.texturePath = "Textures/Brick.png";
		sprite.filterMethod = Texture::FilterMethod::Linear;
		sprite.wrapMethod = Texture::WrapMethod::Mirror;

		AnimatedSpriteComponent& animatedSprite = root.AddComponent<AnimatedSpriteComponent>();
		animatedSprite.tint = Colour(0.25f, 0.5f, 0.75f, 1.0f);
		animatedSprite.spriteSheet = Asset<SpriteSheet>("Sprites/Hero.spritesheet");
		animatedSprite.animation = "Run";

		StaticMeshComponent& staticMesh = root.AddComponent<StaticMeshComponent>();
		staticMesh.mesh = Asset<StaticMesh>("Meshes/Rock.staticmesh");
		staticMesh.materialOverrides = { nullptr, nullptr, Asset<Material>("Materials/Moss.material") };

		PrimitiveComponent& primitive = root.AddComponent<PrimitiveComponent>();
		primitive.material = Asset<Material>("Materials/Wall.material");
		primitive.SetTorus(2.5f, 0.75f, 12);

		TilemapComponent& tilemap = root.AddComponent<TilemapComponent>();
		tilemap.tileset = Asset<Tileset>("Tilesets/Grass.tileset");
		tilemap.tilesWide = 3;
		tilemap.tilesHigh = 2;
		tilemap.tiles = { { 1, 2, 3 }, { 4, 5, 6 } };
		tilemap.tileWidth = 16;
		tilemap.tileHeight = 24;
		tilemap.tint = Colour(0.75f, 0.5f, 0.25f, 1.0f);
		tilemap.orientation = TilemapComponent::Orientation::hexagonal;
		tilemap.isTrigger = true;
		tilemap.layer = 2;
		tilemap.mask = 5;

		TextComponent& text = root.AddComponent<TextComponent>();
		text.text = "Hello, world";
		text.font = Asset<Font>("Fonts/Title.ttf");
		text.maxWidth = 7.5f;
		text.colour = Colour(0.25f, 0.75f, 0.5f, 1.0f);

		RigidBody2DComponent& rigidBody = root.AddComponent<RigidBody2DComponent>();
		rigidBody.type = RigidBody2DComponent::BodyType::DYNAMIC;
		rigidBody.fixedRotation = true;
		rigidBody.gravityScale = 0.5f;
		rigidBody.angularDamping = 0.25f;
		rigidBody.linearDamping = 0.75f;

		Ref<PhysicsMaterial> bouncy = Asset<PhysicsMaterial>("Materials/Bouncy.physicsmaterial");

		BoxCollider2DComponent& box = root.AddComponent<BoxCollider2DComponent>();
		box.physicsMaterial = bouncy;
		box.offset = Vector2f(0.5f, -0.5f);
		box.size = Vector2f(1.5f, 2.5f);
		box.isTrigger = true;
		box.layer = 3;
		box.mask = 6;

		CircleCollider2DComponent& circle = root.AddComponent<CircleCollider2DComponent>();
		circle.physicsMaterial = bouncy;
		circle.radius = 1.25f;
		circle.offset = Vector2f(-1.0f, 1.0f);
		circle.isTrigger = true;
		circle.layer = 4;
		circle.mask = 7;

		PolygonCollider2DComponent& polygon = root.AddComponent<PolygonCollider2DComponent>();
		polygon.physicsMaterial = bouncy;
		polygon.offset = Vector2f(0.25f, 0.75f);
		polygon.vertices = { Vector2f(0.0f, 0.0f), Vector2f(2.0f, 0.0f), Vector2f(2.0f, 1.0f), Vector2f(0.0f, 1.5f) };
		polygon.isTrigger = true;
		polygon.layer = 5;
		polygon.mask = 8;

		CapsuleCollider2DComponent& capsule = root.AddComponent<CapsuleCollider2DComponent>();
		capsule.physicsMaterial = bouncy;
		capsule.direction = CapsuleCollider2DComponent::Direction::Horizontal;
		capsule.radius = 0.75f;
		capsule.height = 3.5f;
		capsule.offset = Vector2f(1.0f, 2.0f);
		capsule.isTrigger = true;
		capsule.layer = 6;
		capsule.mask = 9;

		CircleRendererComponent& circleRenderer = root.AddComponent<CircleRendererComponent>();
		circleRenderer.colour = Colour(1.0f, 0.5f, 0.25f, 0.75f);
		circleRenderer.radius = 2.5f;
		circleRenderer.thickness = 0.25f;
		circleRenderer.fade = 0.125f;

		root.AddComponent<BehaviourTreeComponent>().filepath = "AI/Enemy.behaviourtree";
		root.AddComponent<StateMachineComponent>().filepath = "AI/Door.statemachine";
		root.AddComponent<LuaScriptComponent>().script = Asset<LuaScript>("Scripts/Player.lua");

		PointLightComponent& light = root.AddComponent<PointLightComponent>();
		light.colour = Colour(1.0f, 0.75f, 0.5f, 1.0f);
		light.castsShadows = false;
		light.range = 25.0f;
		light.attenuation = 2.5f;

		root.AddComponent<BillboardComponent>().orientation = BillboardComponent::Orientation::WorldUp;
		root.AddComponent<CanvasComponent>().pixelPerUnit = 32.0f;

		WidgetComponent& widget = root.AddComponent<WidgetComponent>();
		widget.disabled = true;
		widget.fixedWidth = false;
		widget.fixedHeight = false;
		widget.anchorLeft = 0.25f;
		widget.anchorRight = 0.75f;
		widget.anchorTop = 0.125f;
		widget.anchorBottom = 0.875f;
		widget.rotation = 0.5f;
		// The setters keep the margins in step with the anchors, as the editor does
		widget.SetPositionX(20.0f);
		widget.SetPositionY(40.0f);
		widget.SetSizeX(160.0f);
		widget.SetSizeY(48.0f);

		ButtonComponent& button = root.AddComponent<ButtonComponent>();
		button.normalTexture = Texture("Textures/Normal.png");
		button.hoveredTexture = Texture("Textures/Hovered.png");
		button.clickedTexture = Texture("Textures/Clicked.png");
		button.disabledTexture = Texture("Textures/Disabled.png");
		button.normalFilterMethod = Texture::FilterMethod::Linear;
		button.hoveredWrapMethod = Texture::WrapMethod::Clamp;
		button.clickedWrapMethod = Texture::WrapMethod::Mirror;
		button.disabledFilterMethod = Texture::FilterMethod::Linear;
		button.normalTint = Colour(0.5f, 0.5f, 0.5f, 1.0f);
		button.hoveredTint = Colour(0.75f, 0.75f, 0.75f, 1.0f);
		button.clickedTint = Colour(0.25f, 0.25f, 0.25f, 1.0f);
		button.disabledTint = Colour(0.5f, 0.5f, 0.5f, 0.5f);

		StackLayoutComponent& stack = root.AddComponent<StackLayoutComponent>();
		stack.horizontal = true;
		stack.spacing = 6.0f;
		stack.stretchCrossAxis = false;
		stack.paddingLeft = 1.0f;
		stack.paddingTop = 2.0f;
		stack.paddingRight = 3.0f;
		stack.paddingBottom = 4.0f;

		GridLayoutComponent& grid = root.AddComponent<GridLayoutComponent>();
		grid.columns = 4;
		grid.cellSpacing = Vector2f(5.0f, 7.0f);
		grid.uniformCellSize = false;
		grid.fixedRowHeight = 30.0f;
		grid.paddingLeft = 5.0f;
		grid.paddingTop = 6.0f;
		grid.paddingRight = 7.0f;
		grid.paddingBottom = 8.0f;

		ScrollBoxComponent& scrollBox = root.AddComponent<ScrollBoxComponent>();
		scrollBox.scrollOffset = Vector2f(10.0f, 15.0f);
		scrollBox.horizontalScroll = true;
		scrollBox.verticalScroll = false;
		scrollBox.clipContent = false;

		root.AddComponent<HiddenComponent>().hidden = false;

		AudioSourceComponent& audioSource = root.AddComponent<AudioSourceComponent>();
		audioSource.audioClip = Asset<AudioClip>("Audio/Jump.wav");
		audioSource.volume = 0.5f;
		audioSource.pitch = 1.5f;
		audioSource.loop = true;
		audioSource.minDistance = 2.0f;
		audioSource.maxDistance = 20.0f;
		audioSource.rolloff = 0.5f;
		audioSource.stream = true;
		audioSource.playOnStart = true;

		root.AddComponent<AudioListenerComponent>().primary = false;

		Entity child = scene.CreateEntity(Uuid(std::string(k_ChildId)), "Child");
		child.AddComponent<TransformComponent>(Vector3f(5.0f, 6.0f, 7.0f));
		WeldJoint2DComponent& weld = child.AddComponent<WeldJoint2DComponent>();
		weld.collideConnected = true;
		weld.damping = 0.5f;
		weld.stiffness = 4.0f;
		root.AddChild(child);

		return root;
	}

	inline void Check(Entity root)
	{
		REQUIRE(root);
		CHECK(root.GetName() == "Root");
		CHECK(root.GetID() == Uuid(std::string(k_RootId)));

		REQUIRE(root.HasComponent<TransformComponent>());
		const TransformComponent& transform = root.GetComponent<TransformComponent>();
		CHECK(transform.GetPosition() == Vector3f(1.0f, 2.0f, 3.0f));
		CHECK(transform.GetRotation() == Vector3f(0.25f, 0.5f, 0.75f));
		CHECK(transform.GetScale() == Vector3f(2.0f, 3.0f, 4.0f));

		REQUIRE(root.HasComponent<CameraComponent>());
		const CameraComponent& camera = root.GetComponent<CameraComponent>();
		CHECK_FALSE(camera.primary);
		CHECK(camera.fixedAspectRatio);
		CHECK(camera.camera.GetProjectionType() == SceneCamera::ProjectionType::orthographic);
		CHECK(camera.camera.GetOrthoSize() == 12.0f);
		CHECK(camera.camera.GetOrthoNear() == -2.0f);
		CHECK(camera.camera.GetOrthoFar() == 3.0f);
		CHECK(camera.camera.GetPerspectiveNear() == 0.5f);
		CHECK(camera.camera.GetPerspectiveFar() == 250.0f);
		CHECK(camera.camera.GetVerticalFov() == 0.75f);
		CHECK(camera.camera.GetAspectRatio() == 2.0f);

		REQUIRE(root.HasComponent<SpriteComponent>());
		SpriteComponent& sprite = root.GetComponent<SpriteComponent>();
		CHECK(sprite.tint == Colour(0.5f, 0.25f, 0.75f, 0.5f));
		CHECK(sprite.tilingFactor == 3.0f);
		CHECK(HasAsset(sprite.GetTexture(), "Textures/Brick.png"));
		CHECK(sprite.filterMethod == Texture::FilterMethod::Linear);
		CHECK(sprite.wrapMethod == Texture::WrapMethod::Mirror);

		REQUIRE(root.HasComponent<AnimatedSpriteComponent>());
		const AnimatedSpriteComponent& animatedSprite = root.GetComponent<AnimatedSpriteComponent>();
		CHECK(animatedSprite.tint == Colour(0.25f, 0.5f, 0.75f, 1.0f));
		CHECK(HasAsset(animatedSprite.spriteSheet, "Sprites/Hero.spritesheet"));
		CHECK(animatedSprite.animation == "Run");

		REQUIRE(root.HasComponent<StaticMeshComponent>());
		const StaticMeshComponent& staticMesh = root.GetComponent<StaticMeshComponent>();
		CHECK(HasAsset(staticMesh.mesh, "Meshes/Rock.staticmesh"));
		REQUIRE(staticMesh.materialOverrides.size() == 3);
		CHECK(staticMesh.materialOverrides[0] == nullptr);
		CHECK(HasAsset(staticMesh.materialOverrides[2], "Materials/Moss.material"));

		REQUIRE(root.HasComponent<PrimitiveComponent>());
		const PrimitiveComponent& primitive = root.GetComponent<PrimitiveComponent>();
		CHECK(HasAsset(primitive.material, "Materials/Wall.material"));
		CHECK(primitive.type == PrimitiveComponent::Shape::Torus);
		CHECK(primitive.torusOuterRadius == 2.5f);
		CHECK(primitive.torusInnerRadius == 0.75f);
		CHECK(primitive.torusSliceCount == 12);

		REQUIRE(root.HasComponent<TilemapComponent>());
		const TilemapComponent& tilemap = root.GetComponent<TilemapComponent>();
		CHECK(HasAsset(tilemap.tileset, "Tilesets/Grass.tileset"));
		CHECK(tilemap.tilesWide == 3);
		CHECK(tilemap.tilesHigh == 2);
		CHECK(tilemap.tiles == std::vector<std::vector<uint32_t>>{ { 1, 2, 3 }, { 4, 5, 6 } });
		CHECK(tilemap.tileWidth == 16);
		CHECK(tilemap.tileHeight == 24);
		CHECK(tilemap.tint == Colour(0.75f, 0.5f, 0.25f, 1.0f));
		CHECK(tilemap.orientation == TilemapComponent::Orientation::hexagonal);
		CHECK(tilemap.isTrigger);
		CHECK(tilemap.layer == 2);
		CHECK(tilemap.mask == 5);

		REQUIRE(root.HasComponent<TextComponent>());
		const TextComponent& text = root.GetComponent<TextComponent>();
		CHECK(text.text == "Hello, world");
		CHECK(HasAsset(text.font, "Fonts/Title.ttf"));
		CHECK(text.maxWidth == 7.5f);
		CHECK(text.colour == Colour(0.25f, 0.75f, 0.5f, 1.0f));

		REQUIRE(root.HasComponent<RigidBody2DComponent>());
		const RigidBody2DComponent& rigidBody = root.GetComponent<RigidBody2DComponent>();
		CHECK(rigidBody.type == RigidBody2DComponent::BodyType::DYNAMIC);
		CHECK(rigidBody.fixedRotation);
		CHECK(rigidBody.gravityScale == 0.5f);
		CHECK(rigidBody.angularDamping == 0.25f);
		CHECK(rigidBody.linearDamping == 0.75f);

		REQUIRE(root.HasComponent<BoxCollider2DComponent>());
		const BoxCollider2DComponent& box = root.GetComponent<BoxCollider2DComponent>();
		CHECK(HasAsset(box.physicsMaterial, "Materials/Bouncy.physicsmaterial"));
		CHECK(box.offset == Vector2f(0.5f, -0.5f));
		CHECK(box.size == Vector2f(1.5f, 2.5f));
		CHECK(box.isTrigger);
		CHECK(box.layer == 3);
		CHECK(box.mask == 6);

		REQUIRE(root.HasComponent<CircleCollider2DComponent>());
		const CircleCollider2DComponent& circle = root.GetComponent<CircleCollider2DComponent>();
		CHECK(HasAsset(circle.physicsMaterial, "Materials/Bouncy.physicsmaterial"));
		CHECK(circle.radius == 1.25f);
		CHECK(circle.offset == Vector2f(-1.0f, 1.0f));
		CHECK(circle.isTrigger);
		CHECK(circle.layer == 4);
		CHECK(circle.mask == 7);

		REQUIRE(root.HasComponent<PolygonCollider2DComponent>());
		const PolygonCollider2DComponent& polygon = root.GetComponent<PolygonCollider2DComponent>();
		CHECK(HasAsset(polygon.physicsMaterial, "Materials/Bouncy.physicsmaterial"));
		CHECK(polygon.offset == Vector2f(0.25f, 0.75f));
		REQUIRE(polygon.vertices.size() == 4);
		CHECK(polygon.vertices[1] == Vector2f(2.0f, 0.0f));
		CHECK(polygon.vertices[3] == Vector2f(0.0f, 1.5f));
		CHECK(polygon.isTrigger);
		CHECK(polygon.layer == 5);
		CHECK(polygon.mask == 8);

		REQUIRE(root.HasComponent<CapsuleCollider2DComponent>());
		const CapsuleCollider2DComponent& capsule = root.GetComponent<CapsuleCollider2DComponent>();
		CHECK(HasAsset(capsule.physicsMaterial, "Materials/Bouncy.physicsmaterial"));
		CHECK(capsule.direction == CapsuleCollider2DComponent::Direction::Horizontal);
		CHECK(capsule.radius == 0.75f);
		CHECK(capsule.height == 3.5f);
		CHECK(capsule.offset == Vector2f(1.0f, 2.0f));
		CHECK(capsule.isTrigger);
		CHECK(capsule.layer == 6);
		CHECK(capsule.mask == 9);

		REQUIRE(root.HasComponent<CircleRendererComponent>());
		const CircleRendererComponent& circleRenderer = root.GetComponent<CircleRendererComponent>();
		CHECK(circleRenderer.colour == Colour(1.0f, 0.5f, 0.25f, 0.75f));
		CHECK(circleRenderer.radius == 2.5f);
		CHECK(circleRenderer.thickness == 0.25f);
		CHECK(circleRenderer.fade == 0.125f);

		REQUIRE(root.HasComponent<BehaviourTreeComponent>());
		CHECK(SamePath(root.GetComponent<BehaviourTreeComponent>().filepath, "AI/Enemy.behaviourtree"));
		REQUIRE(root.HasComponent<StateMachineComponent>());
		CHECK(SamePath(root.GetComponent<StateMachineComponent>().filepath, "AI/Door.statemachine"));
		REQUIRE(root.HasComponent<LuaScriptComponent>());
		CHECK(HasAsset(root.GetComponent<LuaScriptComponent>().script, "Scripts/Player.lua"));

		REQUIRE(root.HasComponent<PointLightComponent>());
		const PointLightComponent& light = root.GetComponent<PointLightComponent>();
		CHECK(light.colour == Colour(1.0f, 0.75f, 0.5f, 1.0f));
		CHECK_FALSE(light.castsShadows);
		CHECK(light.range == 25.0f);
		CHECK(light.attenuation == 2.5f);

		REQUIRE(root.HasComponent<BillboardComponent>());
		CHECK(root.GetComponent<BillboardComponent>().orientation == BillboardComponent::Orientation::WorldUp);
		REQUIRE(root.HasComponent<CanvasComponent>());
		CHECK(root.GetComponent<CanvasComponent>().pixelPerUnit == 32.0f);

		REQUIRE(root.HasComponent<WidgetComponent>());
		const WidgetComponent& widget = root.GetComponent<WidgetComponent>();
		CHECK(widget.disabled);
		CHECK_FALSE(widget.fixedWidth);
		CHECK_FALSE(widget.fixedHeight);
		CHECK(widget.anchorLeft == 0.25f);
		CHECK(widget.anchorRight == 0.75f);
		CHECK(widget.anchorTop == 0.125f);
		CHECK(widget.anchorBottom == 0.875f);
		CHECK(widget.marginLeft == -460.0f);
		CHECK(widget.marginRight == -300.0f);
		CHECK(widget.marginTop == -95.0f);
		CHECK(widget.marginBottom == -47.0f);
		CHECK(widget.rotation == 0.5f);
		CHECK(widget.position == Vector2f(20.0f, 40.0f));
		CHECK(widget.size == Vector2f(160.0f, 48.0f));

		REQUIRE(root.HasComponent<ButtonComponent>());
		const ButtonComponent& button = root.GetComponent<ButtonComponent>();
		CHECK(HasAsset(button.normalTexture, "Textures/Normal.png"));
		CHECK(HasAsset(button.hoveredTexture, "Textures/Hovered.png"));
		CHECK(HasAsset(button.clickedTexture, "Textures/Clicked.png"));
		CHECK(HasAsset(button.disabledTexture, "Textures/Disabled.png"));
		CHECK(button.normalFilterMethod == Texture::FilterMethod::Linear);
		CHECK(button.hoveredWrapMethod == Texture::WrapMethod::Clamp);
		CHECK(button.clickedWrapMethod == Texture::WrapMethod::Mirror);
		CHECK(button.disabledFilterMethod == Texture::FilterMethod::Linear);
		CHECK(button.normalTint == Colour(0.5f, 0.5f, 0.5f, 1.0f));
		CHECK(button.hoveredTint == Colour(0.75f, 0.75f, 0.75f, 1.0f));
		CHECK(button.clickedTint == Colour(0.25f, 0.25f, 0.25f, 1.0f));
		CHECK(button.disabledTint == Colour(0.5f, 0.5f, 0.5f, 0.5f));

		REQUIRE(root.HasComponent<StackLayoutComponent>());
		const StackLayoutComponent& stack = root.GetComponent<StackLayoutComponent>();
		CHECK(stack.horizontal);
		CHECK(stack.spacing == 6.0f);
		CHECK_FALSE(stack.stretchCrossAxis);
		CHECK(stack.paddingLeft == 1.0f);
		CHECK(stack.paddingTop == 2.0f);
		CHECK(stack.paddingRight == 3.0f);
		CHECK(stack.paddingBottom == 4.0f);

		REQUIRE(root.HasComponent<GridLayoutComponent>());
		const GridLayoutComponent& grid = root.GetComponent<GridLayoutComponent>();
		CHECK(grid.columns == 4);
		CHECK(grid.cellSpacing == Vector2f(5.0f, 7.0f));
		CHECK_FALSE(grid.uniformCellSize);
		CHECK(grid.fixedRowHeight == 30.0f);
		CHECK(grid.paddingLeft == 5.0f);
		CHECK(grid.paddingTop == 6.0f);
		CHECK(grid.paddingRight == 7.0f);
		CHECK(grid.paddingBottom == 8.0f);

		REQUIRE(root.HasComponent<ScrollBoxComponent>());
		const ScrollBoxComponent& scrollBox = root.GetComponent<ScrollBoxComponent>();
		CHECK(scrollBox.scrollOffset == Vector2f(10.0f, 15.0f));
		CHECK(scrollBox.horizontalScroll);
		CHECK_FALSE(scrollBox.verticalScroll);
		CHECK_FALSE(scrollBox.clipContent);

		REQUIRE(root.HasComponent<HiddenComponent>());
		CHECK_FALSE(root.GetComponent<HiddenComponent>().hidden);

		REQUIRE(root.HasComponent<AudioSourceComponent>());
		const AudioSourceComponent& audioSource = root.GetComponent<AudioSourceComponent>();
		CHECK(HasAsset(audioSource.audioClip, "Audio/Jump.wav"));
		CHECK(audioSource.volume == 0.5f);
		CHECK(audioSource.pitch == 1.5f);
		CHECK(audioSource.loop);
		CHECK(audioSource.minDistance == 2.0f);
		CHECK(audioSource.maxDistance == 20.0f);
		CHECK(audioSource.rolloff == 0.5f);
		CHECK(audioSource.stream);
		CHECK(audioSource.playOnStart);

		REQUIRE(root.HasComponent<AudioListenerComponent>());
		CHECK_FALSE(root.GetComponent<AudioListenerComponent>().primary);

		Entity child = root.GetChild();
		REQUIRE(child);
		CHECK(child.GetName() == "Child");
		CHECK(child.GetID() == Uuid(std::string(k_ChildId)));
		CHECK(child.GetParent() == root);
		REQUIRE(child.HasComponent<TransformComponent>());
		CHECK(child.GetComponent<TransformComponent>().GetPosition() == Vector3f(5.0f, 6.0f, 7.0f));
		REQUIRE(child.HasComponent<WeldJoint2DComponent>());
		const WeldJoint2DComponent& weld = child.GetComponent<WeldJoint2DComponent>();
		CHECK(weld.collideConnected);
		CHECK(weld.damping == 0.5f);
		CHECK(weld.stiffness == 4.0f);
	}
}
