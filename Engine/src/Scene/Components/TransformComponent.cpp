#include "TransformComponent.h"

#include "Reflection/Describe.h"
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

void TransformComponent::Describe(Reflection::Builder<TransformComponent>& c)
{
	c.Name("Transform").Category("Core").Required()
		.Description("Position, rotation and scale relative to the parent entity");

	c.Property<&TransformComponent::GetPosition, &TransformComponent::SetPosition>("Position",
		"Position relative to the parent entity, or to the world if it has no parent");
	// Euler angles until quaternion-rotation.md stores a quaternion
	c.Property<&TransformComponent::GetRotation, &TransformComponent::SetRotation>("Rotation",
		"Rotation as Euler angles in radians, relative to the parent");
	c.Property<&TransformComponent::GetScale, &TransformComponent::SetScale>("Scale", "Scale relative to the parent");

	c.OnChanged(&TransformComponentReflection::TeleportBody);
}
