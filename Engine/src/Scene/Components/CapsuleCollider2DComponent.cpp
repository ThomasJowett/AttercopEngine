#include "CapsuleCollider2DComponent.h"

#include "Reflection/Describe.h"
#include "Physics/PhysicsEngine2D.h"

namespace CapsuleCollider2DComponentReflection
{
	void RebuildBody(Entity entity, CapsuleCollider2DComponent&)
	{
		if (PhysicsEngine2D* physics = entity.GetScene()->GetPhysicsEngine2D())
			physics->RebuildBody(entity);
	}
}

void CapsuleCollider2DComponent::Describe(Reflection::Builder<CapsuleCollider2DComponent>& c)
{
	using namespace CapsuleCollider2DComponentReflection;

	Reflection::Enum<Direction>("CapsuleDirection", "Capsule directions, for CapsuleCollider2DComponent.Direction", {
		{ "Vertical", Direction::Vertical },
		{ "Horizontal", Direction::Horizontal },
	});

	c.Name("CapsuleCollider2D").Category("Physics 2D")
		.Description("A capsule-shaped collision shape: a rectangle with rounded ends");

	c.Field<&CapsuleCollider2DComponent::physicsMaterial>("PhysicsMaterial", "Density, friction and restitution of the shape, or nil for the defaults");
	c.Field<&CapsuleCollider2DComponent::direction>("Direction", "Whether the capsule's length runs up or across");
	c.Field<&CapsuleCollider2DComponent::radius>("Radius", "Radius of the capsule's rounded ends");
	c.Field<&CapsuleCollider2DComponent::height>("Height", "Total height of the capsule");
	c.Field<&CapsuleCollider2DComponent::offset>("Offset", "Offset of the shape from the entity's position");
	c.Field<&CapsuleCollider2DComponent::isTrigger>("IsTrigger", "Whether the shape only reports overlaps instead of physically blocking");
	c.Layer<&CapsuleCollider2DComponent::layer>("Layer", "The collision layer this collider is on");
	c.Mask<&CapsuleCollider2DComponent::mask>("Mask", "The collision layers this collider collides with");

	c.OnChanged(&RebuildBody);
}
