#include "GridLayoutComponent.h"

#include "Reflection/Describe.h"

void GridLayoutComponent::Describe(Reflection::Builder<GridLayoutComponent>& c)
{
	c.Name("GridLayout").Category("UI")
		.Description("Lays the widget's children out in a grid");

	c.Field<&GridLayoutComponent::columns>("Columns", "Number of columns").Range(1.0f, 64.0f).Speed(1.0f);
	c.Field<&GridLayoutComponent::cellSpacing>("CellSpacing", "Gap between cells, in pixels");
	c.Field<&GridLayoutComponent::uniformCellSize>("UniformCellSize", "Make every cell the size of the largest child");
	c.Field<&GridLayoutComponent::fixedRowHeight>("FixedRowHeight", "Height of every row in pixels, or 0 to fit each row's children");
	c.Field<&GridLayoutComponent::paddingLeft>("PaddingLeft", "Space inside the left edge, in pixels");
	c.Field<&GridLayoutComponent::paddingTop>("PaddingTop", "Space inside the top edge, in pixels");
	c.Field<&GridLayoutComponent::paddingRight>("PaddingRight", "Space inside the right edge, in pixels");
	c.Field<&GridLayoutComponent::paddingBottom>("PaddingBottom", "Space inside the bottom edge, in pixels");
}
