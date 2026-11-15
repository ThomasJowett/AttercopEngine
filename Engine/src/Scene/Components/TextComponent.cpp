#include "TextComponent.h"

#include "Reflection/Describe.h"

void TextComponent::Describe(Reflection::Builder<TextComponent>& c)
{
	c.Name("Text").Category("Rendering 2D")
		.Description("Draws a string in a font, wrapped at a maximum width");

	c.Field<&TextComponent::text>("Text", "The string to draw").AsText();
	c.Field<&TextComponent::maxWidth>("MaxWidth", "Width at which lines wrap, in world units");
	c.Field<&TextComponent::colour>("Colour", "Colour of the text");
	c.Field<&TextComponent::font>("Font", "The font the text is drawn with");
}
