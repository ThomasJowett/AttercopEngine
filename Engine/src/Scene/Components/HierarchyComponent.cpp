#include "HierarchyComponent.h"

#include "Reflection/Describe.h"

void HierarchyComponent::Describe(Reflection::Builder<HierarchyComponent>& c)
{
	// The links are handles, saved as the nesting of <Entity> elements
	c.Name("Hierarchy").Category("Core").Internal()
		.Description("The entity's parent, children and siblings");
}
