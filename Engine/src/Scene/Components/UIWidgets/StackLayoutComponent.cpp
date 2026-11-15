#include "StackLayoutComponent.h"

#include "Reflection/Describe.h"

void StackLayoutComponent::Describe(Reflection::Builder<StackLayoutComponent>& c)
{
	c.Name("StackLayout").Category("UI")
		.Description("Lays the widget's children out in a row or a column");

	c.Field<&StackLayoutComponent::horizontal>("Horizontal", "Lay children out in a row instead of a column");
	c.Field<&StackLayoutComponent::spacing>("Spacing", "Gap between children, in pixels");
	c.Field<&StackLayoutComponent::stretchCrossAxis>("StretchCrossAxis", "Stretch children to fill the other direction");
	c.Field<&StackLayoutComponent::paddingLeft>("PaddingLeft", "Space inside the left edge, in pixels");
	c.Field<&StackLayoutComponent::paddingTop>("PaddingTop", "Space inside the top edge, in pixels");
	c.Field<&StackLayoutComponent::paddingRight>("PaddingRight", "Space inside the right edge, in pixels");
	c.Field<&StackLayoutComponent::paddingBottom>("PaddingBottom", "Space inside the bottom edge, in pixels");
}
