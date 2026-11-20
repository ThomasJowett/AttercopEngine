#include "CanvasComponent.h"

#include "Reflection/Describe.h"

void CanvasComponent::Describe(Reflection::Builder<CanvasComponent>& c)
{
	c.Name("Canvas").Category("UI")
		.Description("The root of a UI: widgets below it are laid out and drawn on it");

	c.Field<&CanvasComponent::pixelPerUnit>("PixelPerUnit", "Pixels per world unit this canvas's UI is drawn at")
		.Label("Pixels per Unit");
}
