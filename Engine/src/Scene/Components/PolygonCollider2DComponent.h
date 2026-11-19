#pragma once

#include "math/Vector2f.h"
#include "Asset/PhysicsMaterial.h"
#include "Utilities/SerializationUtils.h"

#include <vector>
#include "Physics/CollisionLayers.h"
#include "Physics/RuntimePointer.h"
#include "Reflection/ReflectionFwd.h"

class b2Body;

struct PolygonCollider2DComponent
{
	std::vector<Vector2f> vertices = 
	{
		{0.0f, 1.0f},
		{-0.9510565f, 0.309017f},
		{-0.5877852f, -0.8090171f},
		{0.5877854f,-0.8090169f},
		{0.9510565f, 0.3090171f}
	};

	Vector2f offset;
	bool isTrigger = false;
	uint16_t layer = CollisionLayers::DefaultLayer;
	uint16_t mask = CollisionLayers::Everything;

	Ref<PhysicsMaterial> physicsMaterial;

	RuntimePointer<b2Body> runtimeBody;

	PolygonCollider2DComponent() = default;
	static void Describe(Reflection::Builder<PolygonCollider2DComponent>& c);
};
