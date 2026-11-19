#pragma once

#include "Physics/RuntimePointer.h"

#include "math/Vector2f.h"
#include "Asset/PhysicsMaterial.h"
#include "Utilities/SerializationUtils.h"
#include "Physics/CollisionLayers.h"
#include "Reflection/ReflectionFwd.h"

class b2Body;

struct CircleCollider2DComponent
{
  Vector2f offset = { 0.0f,0.0f };

  float radius = 0.5f;

  bool isTrigger = false;
  uint16_t layer = CollisionLayers::DefaultLayer;
  uint16_t mask = CollisionLayers::Everything;

  Ref<PhysicsMaterial> physicsMaterial;

  RuntimePointer<b2Body> runtimeBody;

  CircleCollider2DComponent() = default;

  static void Describe(Reflection::Builder<CircleCollider2DComponent>& c);
};