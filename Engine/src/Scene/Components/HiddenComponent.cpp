#include "HiddenComponent.h"

#include "Reflection/Describe.h"

void HiddenComponent::Describe(Reflection::Builder<HiddenComponent>& c)
{
	c.Name("Hidden").Category("Rendering")
		.Description("Hides the entity and its children from rendering");

	c.Field<&HiddenComponent::hidden>("Hidden", "Whether this entity is hidden from rendering - a hidden parent also hides its children, but doesn't change their own Hidden value");
}
