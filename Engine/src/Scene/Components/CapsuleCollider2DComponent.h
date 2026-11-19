#pragma once

#include "Physics/RuntimePointer.h"

#include "math/Vector2f.h"

#include "Asset/PhysicsMaterial.h"
#include "Utilities/SerializationUtils.h"
#include "Physics/CollisionLayers.h"
#include "Reflection/ReflectionFwd.h"

class b2Body;

struct CapsuleCollider2DComponent
{
	enum class Direction
	{
		Vertical,
		Horizontal
	}direction;

	Vector2f offset = { 0.0f , 0.0f };

	float radius = 0.5f;
	float height = 2.0f;

	bool isTrigger = false;
	uint16_t layer = CollisionLayers::DefaultLayer;
	uint16_t mask = CollisionLayers::Everything;

	Ref<PhysicsMaterial> physicsMaterial;

	RuntimePointer<b2Body> runtimeBody;

	CapsuleCollider2DComponent() = default;
	static void Describe(Reflection::Builder<CapsuleCollider2DComponent>& c);
};
