#pragma once

#include "EnTT/entt.hpp"
#include "math/Vector2f.h"
#include "Physics/RuntimePointer.h"
#include "Scene/EntityReference.h"
#include "Reflection/ReflectionFwd.h"

class b2Body;
class b2WeldJoint;

struct WeldJoint2DComponent
{
	// Unset welds to the parent, or to the world with no parent
	EntityReference connectedEntity;
	bool collideConnected = false;
	float damping = 0.0f;
	float stiffness = 0.0f;

	entt::entity entityA = entt::null;
	// The parent when Play started, welded to when no entity is connected
	entt::entity entityB = entt::null;

	b2Body* bodyA = nullptr;
	b2Body* bodyB = nullptr;

	RuntimePointer<b2WeldJoint> joint;
	static void Describe(Reflection::Builder<WeldJoint2DComponent>& c);
};
