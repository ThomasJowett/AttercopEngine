#pragma once

#include "EnTT/entt.hpp"
#include "math/Vector2f.h"
#include "Physics/RuntimePointer.h"
#include "Reflection/ReflectionFwd.h"

class b2Body;
class b2WeldJoint;

struct WeldJoint2DComponent
{
	bool collideConnected = false;
	float damping = 0.0f;
	float stiffness = 0.0f;

	entt::entity entityA;
	entt::entity entityB;

	b2Body* bodyA = nullptr;
	b2Body* bodyB = nullptr;

	RuntimePointer<b2WeldJoint> joint;
	static void Describe(Reflection::Builder<WeldJoint2DComponent>& c);
};
