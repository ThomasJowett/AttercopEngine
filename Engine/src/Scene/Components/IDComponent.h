#pragma once

#include "Core/UUID.h"

#include "cereal/cereal.hpp"
#include "Reflection/ReflectionFwd.h"

struct IDComponent
{
	Uuid ID;

	IDComponent() = default;
	IDComponent(const IDComponent& other)
		:ID(other.ID) {}
	IDComponent(Uuid id)
		:ID(id) {}

	static void Describe(Reflection::Builder<IDComponent>& c);

private:
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

				ID = Uuid(uuid);
		}
};
