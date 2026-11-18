#pragma once

#include "Core/UUID.h"

#include "cereal/cereal.hpp"
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
	friend cereal::access;
		template <typename Archive>
		void save(Archive& archive) const
		{
				std::string uuid = ID.to_string();
				archive(uuid);
		}

		template <typename Archive>
		void load(Archive& archive)
		{
				std::string uuid;
				archive(uuid);

				ID = Uuid::Parse(uuid).value_or(Uuid::Nil());
		}
};
