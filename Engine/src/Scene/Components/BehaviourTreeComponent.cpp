#include "BehaviourTreeComponent.h"

#include "Reflection/Describe.h"

namespace BehaviourTreeComponentReflection
{
	void Load(Entity, BehaviourTreeComponent& c)
	{
		c.behaviourTree = c.filepath.empty() ? nullptr : BehaviourTree::Serializer::Load(c.filepath);
	}
}

void BehaviourTreeComponent::Describe(Reflection::Builder<BehaviourTreeComponent>& c)
{
	c.Name("BehaviourTree").Category("AI")
		.Description("Runs a behaviour tree, with its own blackboard for this entity");

	c.Field<&BehaviourTreeComponent::filepath>("Filepath", "The .behaviourtree file to run")
		.MainAsset().AssetType("BehaviourTree").OnChanged(&BehaviourTreeComponentReflection::Load);
}
