#pragma once

#include "cereal/cereal.hpp"
#include "Reflection/ReflectionFwd.h"

struct BillboardComponent
{
	enum class Orientation
	{
		WorldUp,
		Camera
	};

	BillboardComponent() = default;
	BillboardComponent(const BillboardComponent&) = default;

	Orientation orientation = Orientation::Camera;
	static void Describe(Reflection::Builder<BillboardComponent>& c);

private:

	friend cereal::access;
	template<typename Archive>
	void serialize(Archive& archive)
	{
		archive(orientation);
	}
};