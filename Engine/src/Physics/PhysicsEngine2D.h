#pragma once
#include "Core/core.h"
#include "Contact2D.h"

class Entity;
struct RigidBody2DComponent;
class Scene;
class b2World;
class b2Draw;
class b2DestructionListener;
struct HitResult2D;


class PhysicsEngine2D
{
public:
	PhysicsEngine2D(const Vector2f& gravity, Scene* scene);
	~PhysicsEngine2D();
	b2World* GetWorld() { return m_Box2DWorld.get(); }

	void OnFixedUpdate();
	void OnRender();

	void RemoveOldContacts();
	ContactListener2D* GetContactListener() { return m_ContactListener.get(); }

	// Creates the bodies and joints for an entity and everything below it
	void InitializeEntity(Entity root);
	// Creates the bodies for the entities and everything below them, then their joints
	void InitializeEntities(const std::vector<Entity>& roots);
	void DestroyEntity(Entity entity);
	bool HasPhysicsComponents(Entity entity);
	// Registry on_destroy handler for RigidBody2D, collider and tilemap components
	void OnPhysicsComponentRemoved(entt::registry& registry, entt::entity entity);

	// Copies a rigid body's type, damping and gravity scale to its Box2D body
	static void ApplyRigidBodySettings(RigidBody2DComponent& rigidBodyComp);
	// Recreates an entity's body and fixtures from its components, keeping its velocity
	void RebuildBody(Entity entity);
	// Replaces an entity's weld joint with one made from its component as it is now
	void RebuildWeldJoint(Entity entity);

	void SetGravity(Vector2f gravity);

	HitResult2D RayCast(Vector2f begin, Vector2f end);
	std::vector<HitResult2D> MultiRayCast2D(Vector2f begin, Vector2f end);
	std::vector<Entity> QueryPoint(Vector2f point);
	void ShowDebugDraw(bool show);


private:
	void InitializeBody(Entity entity);
	// Welds to the connected entity's body, or the parent's, or the world; warns and makes none if the connected entity
	// is missing, is the entity itself or has no body
	void CreateWeldJoint(Entity entity);

	// Declared before the world so it outlives it
	Scope<b2DestructionListener> m_DestructionListener = nullptr;
	Scope<b2World> m_Box2DWorld = nullptr;
	Scope<b2Draw> m_Box2DDraw = nullptr;

	Scope<ContactListener2D> m_ContactListener = nullptr;

	Scene* m_Scene;

	const int32_t m_VelocityIterations = 6;
	const int32_t m_PositionIterations = 2;

	b2Body* m_WorldBody = nullptr;

	// Entities whose body was destroyed because a physics component was removed; rebuilt next step
	std::vector<entt::entity> m_PendingInitialize;
};
