#include "CircleCollider2DComponent.h"

#include "Reflection/Describe.h"
#include "Physics/CollisionLayers.h"
#include "Scripting/Lua/LuaBindings.h"
#include "Physics/PhysicsEngine2D.h"

namespace CircleCollider2DComponentReflection
{
	void RebuildBody(Entity entity, CircleCollider2DComponent&)
	{
		if (PhysicsEngine2D* physics = entity.GetScene()->GetPhysicsEngine2D())
			physics->RebuildBody(entity);
	}
}

namespace CircleCollider2DComponentReflection
{
	REFLECT_LUA_BEGIN(CircleCollider2DComponent)
		REFLECT_LUA_COLLISION_FILTER()
	REFLECT_LUA_END()
}

void CircleCollider2DComponent::Describe(Reflection::Builder<CircleCollider2DComponent>& c)
{
	c.LuaFunctions(&CircleCollider2DComponentReflection::RegisterLuaFunctions);
	using namespace CircleCollider2DComponentReflection;

	c.Name("CircleCollider2D").Category("Physics 2D")
		.Description("A circular collision shape");

	c.Field<&CircleCollider2DComponent::physicsMaterial>("PhysicsMaterial", "Density, friction and restitution of the shape, or nil for the defaults");
	c.Field<&CircleCollider2DComponent::radius>("Radius", "Radius of the circle")
		.Range(0.0f, 10.0f).Speed(0.01f);
	c.Field<&CircleCollider2DComponent::offset>("Offset", "Offset of the shape from the entity's position");
	c.Field<&CircleCollider2DComponent::isTrigger>("IsTrigger", "Whether the shape only reports overlaps instead of physically blocking");
	c.Layer<&CircleCollider2DComponent::layer>("Layer", "The collision layer this collider is on");
	c.Mask<&CircleCollider2DComponent::mask>("Mask", "The collision layers this collider collides with");

	c.OnChanged(&RebuildBody);
}
