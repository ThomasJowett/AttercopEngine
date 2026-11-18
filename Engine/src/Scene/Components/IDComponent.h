#pragma once

#include "Core/UUID.h"

#include "Reflection/ReflectionFwd.h"

struct IDComponent
{
	IDComponent() = default;
	IDComponent(const IDComponent& other)
		:ID(other.ID) {}
	IDComponent(Uuid id)
		:ID(id) {}

	const Uuid& GetID() const { return ID; }

	static void Describe(Reflection::Builder<IDComponent>& c);

private:
	// Fixed once the entity exists; only the scene sets it, as the entity is added or loaded
	Uuid ID;

	friend class Scene;
};
