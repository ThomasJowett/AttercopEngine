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
			rigidBody->SetTransform(Vector2f(position.x, position.y), transform.GetRotation().z);
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
	// Stored as Euler angles
	c.Property<&TransformComponent::GetRotation, &TransformComponent::SetRotation>("Rotation",
		"Rotation as Euler angles, relative to the parent").Degrees();
	c.Property<&TransformComponent::GetScale, &TransformComponent::SetScale>("Scale", "Scale relative to the parent").LockParts();

	c.OnChanged(&TransformComponentReflection::TeleportBody);
}
