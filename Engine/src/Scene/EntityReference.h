#pragma once

#include "Core/UUID.h"

class Entity;
class Scene;

// Another entity in the same scene, held by its UUID
struct EntityReference
{
	Uuid id = Uuid::Nil();

	bool IsSet() const { return !id.IsNil(); }

	// An invalid Entity if the reference is unset or the entity isn't in the scene
	Entity Resolve(Scene* scene) const;

	bool operator==(const EntityReference& other) const { return id == other.id; }
	bool operator!=(const EntityReference& other) const { return id != other.id; }
};
