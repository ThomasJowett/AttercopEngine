#include "IDComponent.h"

#include "Reflection/Describe.h"

namespace IDComponentReflection
{
	std::string GetID(const IDComponent& component) { return component.ID.to_string(); }
}

void IDComponent::Describe(Reflection::Builder<IDComponent>& c)
{
	c.Name("ID").Category("Core").Internal()
		.Description("The entity's UUID, saved as the ID attribute of its <Entity> element");

	c.Property<&IDComponentReflection::GetID>("ID", "The entity's UUID, as two numbers: low-high").NoLua();
}
