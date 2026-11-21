#include "WidgetComponent.h"

#include "Reflection/Describe.h"

void WidgetComponent::SetAnchorLeft(float left)
{
	anchorLeft = left;

	if (anchorRight < left) {
		anchorRight = left;
		marginRight = (position.x + size.x) - (anchorRight * s_referenceWidth);
	}

	marginLeft = position.x - (anchorLeft * s_referenceWidth);
}

void WidgetComponent::SetAnchorRight(float right)
{
	anchorRight = right;

	if (anchorLeft > right) {
		anchorLeft = right;
		marginLeft = position.x - (anchorLeft * s_referenceWidth);
	}

	marginRight = (position.x + size.x) - (anchorRight * s_referenceWidth);
}

void WidgetComponent::SetAnchorTop(float top)
{
	anchorTop = top;
	if (anchorBottom < top) {
		anchorBottom = top;
		marginBottom = position.y + size.y - (anchorBottom * s_referenceHeight);
	}

	marginTop = position.y - (anchorTop * s_referenceHeight);
}

void WidgetComponent::SetAnchorBottom(float bottom)
{
	anchorBottom = bottom;

	if (anchorTop > bottom) {
		anchorTop = bottom;
		marginTop = position.y - (anchorTop * s_referenceHeight);
	}

	marginBottom = position.y + size.y - (anchorBottom * s_referenceHeight);
}

void WidgetComponent::SetMarginLeft(float left)
{
	marginLeft = left;

	position.x = (anchorLeft * s_referenceWidth) + marginLeft;
	if (!fixedWidth)
		size.x = ((anchorRight *s_referenceWidth) + marginRight) - ((anchorLeft * s_referenceWidth) + marginLeft);
	else
		marginRight = marginLeft + size.x;
}

void WidgetComponent::SetMarginTop(float top)
{
	marginTop = top;
	position.y = (anchorTop * s_referenceHeight) + marginTop;
	if (!fixedHeight)
		size.y = ((anchorBottom * s_referenceHeight) + marginBottom) - ((anchorTop * s_referenceHeight) + marginTop);
	else
		marginBottom = marginTop + size.y;
}

void WidgetComponent::SetMarginRight(float right)
{
	marginRight = right;
	if (!fixedWidth)
		size.x = ((anchorRight * s_referenceWidth) + marginRight) - ((anchorLeft * s_referenceWidth) + marginLeft);
	else {
		position.x = (anchorRight * s_referenceWidth) + marginRight - size.x;
		marginLeft = marginRight - size.x;
	}
}

void WidgetComponent::SetMarginBottom(float bottom)
{
	marginBottom = bottom;
	if (!fixedHeight) {
		size.y = ((anchorBottom * s_referenceHeight) + marginBottom) - ((anchorTop * s_referenceHeight) + marginTop);
	}
	else {
		position.y = (anchorTop * s_referenceHeight) + marginTop - size.y;
		marginTop = marginBottom - size.y;
	}
}

void WidgetComponent::SetPositionX(float x)
{
	position.x = x;

	marginLeft = position.x - (anchorLeft * s_referenceWidth);
	marginRight = marginLeft + size.x;

}

void WidgetComponent::SetPositionY(float y)
{
	position.y = y;

	marginTop = position.y - (anchorTop * s_referenceHeight);
	marginBottom = marginTop + size.y;
}

void WidgetComponent::SetSizeX(float sizeX)
{
	size.x = sizeX;

	marginRight = marginLeft + size.x;
}

void WidgetComponent::SetSizeY(float sizeY)
{
	size.y = sizeY;

	marginBottom = marginTop + size.y;
}

namespace WidgetComponentReflection
{
	Vector2f GetPosition(const WidgetComponent& c) { return c.position; }
	void SetPosition(WidgetComponent& c, const Vector2f& position) { c.SetPositionX(position.x); c.SetPositionY(position.y); }
	Vector2f GetSize(const WidgetComponent& c) { return c.size; }
	void SetSize(WidgetComponent& c, const Vector2f& size) { c.SetSizeX(size.x); c.SetSizeY(size.y); }

	// Edits keep the widget where it is, moving the margins; a fixed size keeps both anchors together
	void EditFixedWidth(WidgetComponent& c, bool fixed) { c.fixedWidth = fixed; c.SetAnchorRight(c.anchorLeft); }
	void EditFixedHeight(WidgetComponent& c, bool fixed) { c.fixedHeight = fixed; c.SetAnchorBottom(c.anchorTop); }
	void EditAnchorLeft(WidgetComponent& c, float left)
	{
		c.SetAnchorLeft(left);
		if (c.fixedWidth)
			c.SetAnchorRight(left);
	}
	void EditAnchorTop(WidgetComponent& c, float top)
	{
		c.SetAnchorTop(top);
		if (c.fixedHeight)
			c.SetAnchorBottom(top);
	}

	bool IsStretchedAcross(const WidgetComponent& c) { return !c.fixedWidth; }
	bool IsStretchedDown(const WidgetComponent& c) { return !c.fixedHeight; }
	bool HasFixedSize(const WidgetComponent& c) { return c.fixedWidth || c.fixedHeight; }
}

void WidgetComponent::Describe(Reflection::Builder<WidgetComponent>& c)
{
	using namespace WidgetComponentReflection;

	c.Name("Widget").Category("UI")
		.Description("Places the entity on its canvas, anchored to its parent widget");

	c.Field<&WidgetComponent::disabled>("Disabled", "Whether the widget ignores input and draws in its disabled state");
	c.Field<&WidgetComponent::fixedWidth>("FixedWidth", "Keep the width when the parent resizes, instead of stretching between the anchors")
		.SetWith<&EditFixedWidth>();
	c.Field<&WidgetComponent::fixedHeight>("FixedHeight", "Keep the height when the parent resizes, instead of stretching between the anchors")
		.SetWith<&EditFixedHeight>();
	// The setters move the margins, so these come before the margins
	c.Property<&GetPosition, &SetPosition>("Position", "The widget's position, in reference-resolution pixels");
	c.Property<&GetSize, &SetSize>("Size", "The widget's size, in reference-resolution pixels")
		.EnableIf(&HasFixedSize);
	c.Field<&WidgetComponent::anchorLeft>("AnchorLeft", "Left anchor, as a fraction of the parent's width")
		.Range(0.0f, 1.0f).Speed(0.01f).SetWith<&EditAnchorLeft>();
	c.Field<&WidgetComponent::anchorRight>("AnchorRight", "Right anchor, as a fraction of the parent's width")
		.Range(0.0f, 1.0f).Speed(0.01f).ShowIf(&IsStretchedAcross).SetWith<&WidgetComponent::SetAnchorRight>();
	c.Field<&WidgetComponent::anchorTop>("AnchorTop", "Top anchor, as a fraction of the parent's height")
		.Range(0.0f, 1.0f).Speed(0.01f).SetWith<&EditAnchorTop>();
	c.Field<&WidgetComponent::anchorBottom>("AnchorBottom", "Bottom anchor, as a fraction of the parent's height")
		.Range(0.0f, 1.0f).Speed(0.01f).ShowIf(&IsStretchedDown).SetWith<&WidgetComponent::SetAnchorBottom>();
	c.Field<&WidgetComponent::marginLeft>("MarginLeft", "Distance from the left anchor to the left edge, in pixels")
		.Speed(1.0f).SetWith<&WidgetComponent::SetMarginLeft>();
	c.Field<&WidgetComponent::marginRight>("MarginRight", "Distance from the right anchor to the right edge, in pixels")
		.Speed(1.0f).SetWith<&WidgetComponent::SetMarginRight>();
	c.Field<&WidgetComponent::marginTop>("MarginTop", "Distance from the top anchor to the top edge, in pixels")
		.Speed(1.0f).SetWith<&WidgetComponent::SetMarginTop>();
	c.Field<&WidgetComponent::marginBottom>("MarginBottom", "Distance from the bottom anchor to the bottom edge, in pixels")
		.Speed(1.0f).SetWith<&WidgetComponent::SetMarginBottom>();
	c.Field<&WidgetComponent::rotation>("Rotation", "Rotation of the widget").Degrees().SavedInDegrees();
}
