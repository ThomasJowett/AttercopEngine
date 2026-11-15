#include "BillboardComponent.h"

#include "Reflection/Describe.h"

void BillboardComponent::Describe(Reflection::Builder<BillboardComponent>& c)
{
	Reflection::Enum<Orientation>("BillboardOrientation", "Ways a billboard turns, for BillboardComponent.Orientation", {
		{ "WorldUp", Orientation::WorldUp },
		{ "Camera", Orientation::Camera },
	});

	c.Name("Billboard").Category("Rendering 3D")
		.Description("Turns the entity to face the camera");

	c.Field<&BillboardComponent::orientation>("Orientation", "Turn around the world's up axis only, or face the camera fully");
}
