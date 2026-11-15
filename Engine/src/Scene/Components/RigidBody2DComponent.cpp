
#include "RigidBody2DComponent.h"

#include "Reflection/Describe.h"
#include "Physics/PhysicsEngine2D.h"

#include "Scene/SceneManager.h"
#include "box2d/box2d.h"
#include "Scene/Entity.h"
#include "Scene/SceneGraph.h"
#include "Math/Triangulation.h"

#include "HierarchyComponent.h"
#include "TransformComponent.h"

void RigidBody2DComponent::ApplyImpulse(Vector2f impulse)
{
	if (runtimeBody)
		runtimeBody->ApplyLinearImpulse(b2Vec2(impulse.x, impulse.y), runtimeBody->GetWorldCenter(), true);
}

void RigidBody2DComponent::ApplyImpulseAtPoint(Vector2f impulse, Vector2f center)
{
	if (runtimeBody)
		runtimeBody->ApplyLinearImpulse(b2Vec2(impulse.x, impulse.y), b2Vec2(center.x, center.y), true);
}

void RigidBody2DComponent::ApplyForce(Vector2f force)
{
	if (runtimeBody)
		runtimeBody->ApplyForce(b2Vec2(force.x, force.y), runtimeBody->GetWorldCenter(), true);
}

void RigidBody2DComponent::ApplyForceAtPoint(Vector2f force, Vector2f center)
{
	if (runtimeBody)
		runtimeBody->ApplyForce(b2Vec2(force.x, force.y), b2Vec2(center.x, center.y), true);
}

void RigidBody2DComponent::ApplyTorque(float torque)
{
	if (runtimeBody)
		runtimeBody->ApplyTorque(torque, true);
}

void RigidBody2DComponent::SetLinearVelocity(Vector2f velocity)
{
	if (runtimeBody)
		runtimeBody->SetLinearVelocity(b2Vec2(velocity.x, velocity.y));
}

Vector2f RigidBody2DComponent::GetLinearVelocity()
{
	// No Box2D body until physics starts, e.g. in OnCreate or just after AddRigidBody2DComponent
	if (!runtimeBody)
		return Vector2f(0.0f, 0.0f);
	const b2Vec2& vel = runtimeBody->GetLinearVelocity();
	return Vector2f(vel.x, vel.y);
}

void RigidBody2DComponent::SetAngularVelocity(float velocity)
{
	if (runtimeBody)
		runtimeBody->SetAngularVelocity(velocity);
}

float RigidBody2DComponent::GetAngularVelocity()
{
	if (!runtimeBody)
		return 0.0f;
	return runtimeBody->GetAngularVelocity();
}

void RigidBody2DComponent::SetTransform(const Vector2f& position, const float& angle)
{
	if (runtimeBody)
	{
		runtimeBody->SetAwake(true);
		runtimeBody->SetTransform(b2Vec2(position.x, position.y), angle);
		runtimeBody->SetLinearVelocity(b2Vec2_zero);
	}
}

void RigidBody2DComponent::GetTransform(Vector2f& position, float& rotation)
{
	if (runtimeBody)
	{
		position = { (float)runtimeBody->GetPosition().x, (float)runtimeBody->GetPosition().y };
		rotation = (float)runtimeBody->GetAngle();
	}
}

namespace RigidBody2DComponentReflection
{
	bool IsDynamic(const RigidBody2DComponent& c) { return c.type == RigidBody2DComponent::BodyType::DYNAMIC; }

	void ApplyToBody(Entity, RigidBody2DComponent& c) { PhysicsEngine2D::ApplyRigidBodySettings(c); }
}

void RigidBody2DComponent::Describe(Reflection::Builder<RigidBody2DComponent>& c)
{
	using namespace RigidBody2DComponentReflection;

	Reflection::Enum<BodyType>("BodyType", "Rigid body types, for RigidBody2DComponent.Type", {
		{ "Static", BodyType::STATIC },
		{ "Kinematic", BodyType::KINEMATIC },
		{ "Dynamic", BodyType::DYNAMIC },
	});

	c.Name("RigidBody2D").Category("Physics 2D")
		.Description("Makes the entity a Box2D body that moves under physics");

	c.Field<&RigidBody2DComponent::type>("Type", "BodyType.Static, BodyType.Kinematic or BodyType.Dynamic");
	c.Field<&RigidBody2DComponent::fixedRotation>("FixedRotation", "Stop the body from rotating").ShowIf(&IsDynamic);
	c.Field<&RigidBody2DComponent::gravityScale>("GravityScale", "Multiplier on the scene's gravity for this body").ShowIf(&IsDynamic);
	c.Field<&RigidBody2DComponent::angularDamping>("AngularDamping", "How quickly the body's spin slows down").ShowIf(&IsDynamic);
	c.Field<&RigidBody2DComponent::linearDamping>("LinearDamping", "How quickly the body's movement slows down").ShowIf(&IsDynamic);

	c.OnChanged(&ApplyToBody);
}
