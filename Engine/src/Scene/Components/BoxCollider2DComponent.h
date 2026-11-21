#pragma once

#include "Physics/RuntimePointer.h"

#include "math/Vector2f.h"
#include "Asset/PhysicsMaterial.h"

#include "Utilities/FileUtils.h"
#include "Utilities/SerializationUtils.h"
#include "Core/Application.h"
#include "Scene/AssetManager.h"
#include "Physics/CollisionLayers.h"
#include "Reflection/ReflectionFwd.h"

#include <cstdint>

class b2Body;

struct BoxCollider2DComponent
{
	Vector2f offset = { 0.0f, 0.0f };
	Vector2f size = { 0.5f, 0.5f };

	bool isTrigger = false;
	uint16_t layer = CollisionLayers::DefaultLayer;
	uint16_t mask = CollisionLayers::Everything;

	Ref<PhysicsMaterial> physicsMaterial;

	RuntimePointer<b2Body> runtimeBody;

	BoxCollider2DComponent() = default;
	static void Describe(Reflection::Builder<BoxCollider2DComponent>& c);
};