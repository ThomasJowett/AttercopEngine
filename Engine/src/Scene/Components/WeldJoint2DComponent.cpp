#include "WeldJoint2DComponent.h"

#include "Physics/PhysicsEngine2D.h"
#include "Reflection/Describe.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/Components/RigidBody2DComponent.h"
#include "Scene/Components/TilemapComponent.h"

namespace WeldJoint2DComponentReflection
{
	void RebuildJoint(Entity entity, WeldJoint2DComponent&)
	{
		if (PhysicsEngine2D* physics = entity.GetScene()->GetPhysicsEngine2D())
			physics->RebuildWeldJoint(entity);
	}
}

void WeldJoint2DComponent::Describe(Reflection::Builder<WeldJoint2DComponent>& c)
{
	c.Name("WeldJoint2D").Category("Physics 2D")
		.Description("Welds the entity's body to the connected entity's body, or to its parent's if none is set, or to the world if it has no parent");

	c.Field<&WeldJoint2DComponent::connectedEntity>("ConnectedEntity",
		"The body this one is welded to. Empty welds it to the parent, or to the world if it has none")
		.RequiresAny<RigidBody2DComponent, TilemapComponent>().OnChanged(&WeldJoint2DComponentReflection::RebuildJoint);
	c.Field<&WeldJoint2DComponent::collideConnected>("CollideConnected", "Whether the two welded bodies can still collide with each other");
	c.Field<&WeldJoint2DComponent::damping>("Damping", "Damping of the joint's spring, if Stiffness is above 0")
		.Range(0.0f, 1.0f).Speed(0.01f);
	c.Field<&WeldJoint2DComponent::stiffness>("Stiffness", "Spring stiffness of the weld; 0 makes it rigid")
		.Range(0.0f, 100.0f).Speed(0.01f);
}
