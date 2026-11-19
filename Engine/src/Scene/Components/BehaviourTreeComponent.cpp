#include "BehaviourTreeComponent.h"

#include "Reflection/Describe.h"
#include "Scripting/Lua/LuaBindings.h"

namespace BehaviourTreeComponentReflection
{
	void Load(Entity, BehaviourTreeComponent& c)
	{
		c.behaviourTree = c.filepath.empty() ? nullptr : BehaviourTree::Serializer::Load(c.filepath);
	}
}

namespace BehaviourTreeComponentReflection
{
	REFLECT_LUA_BEGIN(BehaviourTreeComponent)
		REFLECT_LUA_FUNCTION(GetBlackboard, "Get the blackboard shared by this entity's behaviour tree and its custom tasks, or nil if no tree is loaded")
	REFLECT_LUA_END()
}

void BehaviourTreeComponent::Describe(Reflection::Builder<BehaviourTreeComponent>& c)
{
	c.LuaFunctions(&BehaviourTreeComponentReflection::RegisterLuaFunctions);
	c.Name("BehaviourTree").Category("AI")
		.Description("Runs a behaviour tree, with its own blackboard for this entity");

	c.Field<&BehaviourTreeComponent::filepath>("Filepath", "The .behaviourtree file to run")
		.MainAsset().AssetType("BehaviourTree").OnChanged(&BehaviourTreeComponentReflection::Load);
}
