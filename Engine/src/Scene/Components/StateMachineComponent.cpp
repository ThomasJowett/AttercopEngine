#include "StateMachineComponent.h"

#include "Reflection/Describe.h"
#include "Scripting/Lua/LuaBindings.h"

namespace StateMachineComponentReflection
{
	void Load(Entity, StateMachineComponent& c)
	{
		c.stateMachine = c.filepath.empty() ? nullptr : StateMachineSerializer::Load(c.filepath);
	}
}

namespace StateMachineComponentReflection
{
	REFLECT_LUA_BEGIN(StateMachineComponent)
		REFLECT_LUA_FUNCTION(GetCurrentStateName, "Get the name of the active state, or an empty string before the first update")
		REFLECT_LUA_FUNCTION(GetPreviousStateName, "Get the name of the state that was active before the current one, or an empty string")
		REFLECT_LUA_FUNCTION(ChangeState, "Change to the named state, running OnStateExit on the old state and OnStateEntry on the new one; returns false if there's no state with that name")
	REFLECT_LUA_END()
}

void StateMachineComponent::Describe(Reflection::Builder<StateMachineComponent>& c)
{
	c.LuaFunctions(&StateMachineComponentReflection::RegisterLuaFunctions);
	c.Name("StateMachine").Category("AI")
		.Description("Runs a state machine, with its own current state for this entity");

	c.Field<&StateMachineComponent::filepath>("Filepath", "The .statemachine file to run")
		.MainAsset().AssetType("StateMachine").OnChanged(&StateMachineComponentReflection::Load);
}
