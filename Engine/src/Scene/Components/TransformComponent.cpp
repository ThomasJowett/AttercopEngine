#include "TransformComponent.h"

#include "Reflection/Describe.h"
#include "Scripting/Lua/LuaBindings.h"
#include "RigidBody2DComponent.h"

namespace TransformComponentReflection
{
	// Moves the entity's Box2D body to the new transform while the scene runs
	void TeleportBody(Entity entity, TransformComponent& transform)
	{
		if (!entity.GetScene()->IsRunning())
			return;
		if (RigidBody2DComponent* rigidBody = entity.TryGetComponent<RigidBody2DComponent>())
		{
			const Vector3f& position = transform.GetPosition();
			rigidBody->SetTransform(Vector2f(position.x, position.y), transform.GetAngle2D());
		}
	}
}

namespace TransformComponentReflection
{
	REFLECT_LUA_BEGIN(TransformComponent)
		type.set_function("GetWorldPosition", [](const Lua::ComponentRef& handle)
			{
				Lua::UpdateWorldTransform(handle.scene, handle.entity);
				return handle.As<Self>().GetWorldPosition();
			});
		RegisterLuaApiEntry({ "GetWorldPosition", "Position in world space, after applying every parent's transform", LuaApiEntry::Kind::Function, s_ReflectComponentName, "", true });
		type.set_function("GetWorldRotation", [](const Lua::ComponentRef& handle)
			{
				Lua::UpdateWorldTransform(handle.scene, handle.entity);
				return handle.As<Self>().GetWorldRotation();
			});
		RegisterLuaApiEntry({ "GetWorldRotation", "Rotation in world space as a Quaternion, after applying every parent's transform", LuaApiEntry::Kind::Function, s_ReflectComponentName, "Quaternion", true });
	REFLECT_LUA_END()
}

void TransformComponent::Describe(Reflection::Builder<TransformComponent>& c)
{
	c.LuaFunctions(&TransformComponentReflection::RegisterLuaFunctions);
	// A widget is placed by its canvas instead
	c.Name("Transform").Category("Core").Excludes("Widget")
		.Description("Position, rotation and scale relative to the parent entity");

	c.Property<&TransformComponent::GetPosition, &TransformComponent::SetPosition>("Position",
		"Position relative to the parent entity, or to the world if it has no parent");
	// Stored as Euler angles in radians, saved in degrees
	c.Property<&TransformComponent::GetEulerAngles, &TransformComponent::SetEulerAngles>("Rotation",
		"Rotation as Euler angles, relative to the parent").Degrees().SavedInDegrees();
	// The same rotation as Rotation, so it isn't saved or shown twice
	c.Property<&TransformComponent::GetRotation, &TransformComponent::SetRotation>("Orientation",
		"The same rotation as Rotation, as a unit Quaternion").NotSaved().HideInInspector();
	c.Property<&TransformComponent::GetScale, &TransformComponent::SetScale>("Scale", "Scale relative to the parent").LockParts();

	c.OnChanged(&TransformComponentReflection::TeleportBody);
}
