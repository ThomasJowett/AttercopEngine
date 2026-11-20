#include "CircleRendererComponent.h"

#include "Reflection/Describe.h"

void CircleRendererComponent::Describe(Reflection::Builder<CircleRendererComponent>& c)
{
	c.Name("CircleRenderer").Category("Rendering 2D")
		.Description("Draws a filled circle or a ring");

	c.Field<&CircleRendererComponent::radius>("Radius", "Radius of the circle");
	c.Field<&CircleRendererComponent::thickness>("Thickness", "1 draws a filled circle; smaller values draw a ring")
		.Range(0.0f, 1.0f).Speed(0.025f);
	c.Field<&CircleRendererComponent::fade>("Fade", "Softness of the circle's edge")
		.Range(0.0f, 1.0f).Speed(0.00025f);
	c.Field<&CircleRendererComponent::colour>("Colour", "Colour of the circle");
}
