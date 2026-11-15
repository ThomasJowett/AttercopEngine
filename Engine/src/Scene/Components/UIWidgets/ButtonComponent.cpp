#include "ButtonComponent.h"

#include "Reflection/Describe.h"

void ButtonComponent::Describe(Reflection::Builder<ButtonComponent>& c)
{
	c.Name("Button").Category("UI")
		.Description("A widget that can be hovered and clicked, with a texture and tint for each state");

	c.Field<&ButtonComponent::normalTexture>("NormalTexture", "Texture drawn when the button isn't hovered, clicked or disabled");
	c.Field<&ButtonComponent::hoveredTexture>("HoveredTexture", "Texture drawn while the mouse is over the button");
	c.Field<&ButtonComponent::clickedTexture>("ClickedTexture", "Texture drawn while the button is held down");
	c.Field<&ButtonComponent::disabledTexture>("DisabledTexture", "Texture drawn while the button's widget is disabled");
	c.Field<&ButtonComponent::normalFilterMethod>("NormalFilterMethod", "Filtering of the normal state's texture");
	c.Field<&ButtonComponent::hoveredFilterMethod>("HoveredFilterMethod", "Filtering of the hovered state's texture");
	c.Field<&ButtonComponent::clickedFilterMethod>("ClickedFilterMethod", "Filtering of the clicked state's texture");
	c.Field<&ButtonComponent::disabledFilterMethod>("DisabledFilterMethod", "Filtering of the disabled state's texture");
	c.Field<&ButtonComponent::normalWrapMethod>("NormalWrapMethod", "Wrapping of the normal state's texture");
	c.Field<&ButtonComponent::hoveredWrapMethod>("HoveredWrapMethod", "Wrapping of the hovered state's texture");
	c.Field<&ButtonComponent::clickedWrapMethod>("ClickedWrapMethod", "Wrapping of the clicked state's texture");
	c.Field<&ButtonComponent::disabledWrapMethod>("DisabledWrapMethod", "Wrapping of the disabled state's texture");
	c.Field<&ButtonComponent::normalTint>("NormalTint", "Tint applied to the normal state's texture");
	c.Field<&ButtonComponent::hoveredTint>("HoveredTint", "Tint applied to the hovered state's texture");
	c.Field<&ButtonComponent::clickedTint>("ClickedTint", "Tint applied to the clicked state's texture");
	c.Field<&ButtonComponent::disabledTint>("DisabledTint", "Tint applied to the disabled state's texture");
}
