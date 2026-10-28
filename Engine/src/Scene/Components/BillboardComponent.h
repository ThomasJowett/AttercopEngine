#pragma once

#include "cereal/cereal.hpp"

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
private:

	friend cereal::access;
	template<typename Archive>
	void serialize(Archive& archive)
	{
		archive(orientation);
	}
};