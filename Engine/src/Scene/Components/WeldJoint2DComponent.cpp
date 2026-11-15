#include "WeldJoint2DComponent.h"

#include "Reflection/Describe.h"

void WeldJoint2DComponent::Describe(Reflection::Builder<WeldJoint2DComponent>& c)
{
	c.Name("WeldJoint2D").Category("Physics 2D")
		.Description("Welds the entity's body to its parent's, or to the world if it has no parent");

	c.Field<&WeldJoint2DComponent::collideConnected>("CollideConnected", "Whether the two welded bodies can still collide with each other");
	c.Field<&WeldJoint2DComponent::damping>("Damping", "Damping of the joint's spring, if Stiffness is above 0");
	c.Field<&WeldJoint2DComponent::stiffness>("Stiffness", "Spring stiffness of the weld; 0 makes it rigid");
}
