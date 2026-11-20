#include "PointLightComponent.h"

#include "Reflection/Describe.h"

void PointLightComponent::Describe(Reflection::Builder<PointLightComponent>& c)
{
	c.Name("PointLight").Category("Rendering 3D")
		.Description("A light that shines in every direction from the entity's position");

	c.Field<&PointLightComponent::range>("Range", "Distance the light reaches")
		.Range(0.0f, 1000.0f);
	c.Field<&PointLightComponent::attenuation>("Attenuation", "How quickly the light fades with distance")
		.Range(0.0f, 1.0f).Speed(0.001f);
	c.Field<&PointLightComponent::castsShadows>("CastShadows", "Whether the light casts shadows");
	c.Field<&PointLightComponent::colour>("Colour", "Colour of the light");
}
