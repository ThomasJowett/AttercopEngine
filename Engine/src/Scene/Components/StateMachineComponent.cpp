#include "StateMachineComponent.h"

#include "Reflection/Describe.h"

namespace StateMachineComponentReflection
{
	void Load(Entity, StateMachineComponent& c)
	{
		c.stateMachine = c.filepath.empty() ? nullptr : StateMachineSerializer::Load(c.filepath);
	}
}

void StateMachineComponent::Describe(Reflection::Builder<StateMachineComponent>& c)
{
	c.Name("StateMachine").Category("AI")
		.Description("Runs a state machine, with its own current state for this entity");

	c.Field<&StateMachineComponent::filepath>("Filepath", "The .statemachine file to run")
		.MainAsset().AssetType("StateMachine").OnChanged(&StateMachineComponentReflection::Load);
}
