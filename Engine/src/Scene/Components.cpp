#include "Components.h"

#include "Reflection/Describe.h"

// In the order components are written in a scene file and shown in the inspector
void RegisterComponents()
{
	Reflection::Register<IDComponent>();
	Reflection::Register<NameComponent>();
	Reflection::Register<TransformComponent>();
	Reflection::Register<CameraComponent>();
	Reflection::Register<SpriteComponent>();
	Reflection::Register<AnimatedSpriteComponent>();
	Reflection::Register<StaticMeshComponent>();
	Reflection::Register<PrimitiveComponent>();
	Reflection::Register<TilemapComponent>();
	Reflection::Register<TextComponent>();
	Reflection::Register<RigidBody2DComponent>();
	Reflection::Register<BoxCollider2DComponent>();
	Reflection::Register<CircleCollider2DComponent>();
	Reflection::Register<PolygonCollider2DComponent>();
	Reflection::Register<CapsuleCollider2DComponent>();
	Reflection::Register<WeldJoint2DComponent>();
	Reflection::Register<CircleRendererComponent>();
	Reflection::Register<BehaviourTreeComponent>();
	Reflection::Register<StateMachineComponent>();
	Reflection::Register<LuaScriptComponent>();
	Reflection::Register<PointLightComponent>();
	Reflection::Register<BillboardComponent>();
	Reflection::Register<CanvasComponent>();
	Reflection::Register<WidgetComponent>();
	Reflection::Register<ButtonComponent>();
	Reflection::Register<StackLayoutComponent>();
	Reflection::Register<GridLayoutComponent>();
	Reflection::Register<ScrollBoxComponent>();
	Reflection::Register<HiddenComponent>();
	Reflection::Register<AudioSourceComponent>();
	Reflection::Register<AudioListenerComponent>();
	// Its children are written inside the entity, after its components
	Reflection::Register<HierarchyComponent>();
}
