#include "BoxCollider2DComponent.h"

#include "Reflection/Describe.h"
#include "Physics/CollisionLayers.h"
#include "Scripting/Lua/LuaBindings.h"
#include "Physics/PhysicsEngine2D.h"

namespace BoxCollider2DComponentReflection
{
	void RebuildBody(Entity entity, BoxCollider2DComponent&)
	{
		if (PhysicsEngine2D* physics = entity.GetScene()->GetPhysicsEngine2D())
			physics->RebuildBody(entity);
	}
}

namespace BoxCollider2DComponentReflection
{
	REFLECT_LUA_BEGIN(BoxCollider2DComponent)
		REFLECT_LUA_COLLISION_FILTER()
	REFLECT_LUA_END()
}

void BoxCollider2DComponent::Describe(Reflection::Builder<BoxCollider2DComponent>& c)
{
	c.LuaFunctions(&BoxCollider2DComponentReflection::RegisterLuaFunctions);
	using namespace BoxCollider2DComponentReflection;

	c.Name("BoxCollider2D").Category("Physics 2D")
		.Description("A box-shaped collision shape");

	c.Field<&BoxCollider2DComponent::physicsMaterial>("PhysicsMaterial", "Density, friction and restitution of the shape, or nil for the defaults");
	c.Field<&BoxCollider2DComponent::offset>("Offset", "Offset of the shape from the entity's position");
	c.Field<&BoxCollider2DComponent::size>("Size", "Half the width and height of the box");
	c.Field<&BoxCollider2DComponent::isTrigger>("IsTrigger", "Whether the shape only reports overlaps instead of physically blocking");
	c.Layer<&BoxCollider2DComponent::layer>("Layer", "The collision layer this collider is on");
	c.Mask<&BoxCollider2DComponent::mask>("Mask", "The collision layers this collider collides with");

	c.OnChanged(&RebuildBody);
}
