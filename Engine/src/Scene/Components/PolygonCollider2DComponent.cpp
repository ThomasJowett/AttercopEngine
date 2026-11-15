#include "PolygonCollider2DComponent.h"

#include "Reflection/Describe.h"
#include "Physics/PhysicsEngine2D.h"

namespace PolygonCollider2DComponentReflection
{
	void RebuildBody(Entity entity, PolygonCollider2DComponent&)
	{
		if (PhysicsEngine2D* physics = entity.GetScene()->GetPhysicsEngine2D())
			physics->RebuildBody(entity);
	}
}

void PolygonCollider2DComponent::Describe(Reflection::Builder<PolygonCollider2DComponent>& c)
{
	using namespace PolygonCollider2DComponentReflection;

	c.Name("PolygonCollider2D").Category("Physics 2D")
		.Description("A collision shape with any number of corners");

	c.Field<&PolygonCollider2DComponent::physicsMaterial>("PhysicsMaterial", "Density, friction and restitution of the shape, or nil for the defaults");
	c.Field<&PolygonCollider2DComponent::offset>("Offset", "Offset of the shape from the entity's position");
	c.Field<&PolygonCollider2DComponent::vertices>("Vertices", "The polygon's corners, relative to the entity's position").Item("Vertex");
	c.Field<&PolygonCollider2DComponent::isTrigger>("IsTrigger", "Whether the shape only reports overlaps instead of physically blocking");
	c.Layer<&PolygonCollider2DComponent::layer>("Layer", "The collision layer this collider is on");
	c.Mask<&PolygonCollider2DComponent::mask>("Mask", "The collision layers this collider collides with");

	c.OnChanged(&RebuildBody);
}
