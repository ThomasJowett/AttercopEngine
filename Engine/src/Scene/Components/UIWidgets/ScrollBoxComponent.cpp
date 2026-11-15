#include "ScrollBoxComponent.h"

#include "Reflection/Describe.h"

void ScrollBoxComponent::Describe(Reflection::Builder<ScrollBoxComponent>& c)
{
	c.Name("ScrollBox").Category("UI")
		.Description("Scrolls its one child, the content, inside the widget");

	c.Field<&ScrollBoxComponent::scrollOffset>("ScrollOffset", "How far the content is scrolled, in pixels");
	c.Field<&ScrollBoxComponent::horizontalScroll>("HorizontalScroll", "Whether the content scrolls across");
	c.Field<&ScrollBoxComponent::verticalScroll>("VerticalScroll", "Whether the content scrolls up and down");
	c.Field<&ScrollBoxComponent::clipContent>("ClipContent", "Hide the parts of the content outside the box");
}
