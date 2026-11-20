#include "NameComponent.h"

#include "Reflection/Describe.h"

void NameComponent::Describe(Reflection::Builder<NameComponent>& c)
{
	c.Name("Name").Category("Core").Internal()
		.Description("The entity's name, saved as the Name attribute of its <Entity> element");

	c.Field<&NameComponent::name>("Name", "The name of this entity").Label("Entity Name");
}
