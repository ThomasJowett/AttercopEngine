#include "vendor/doctest.h"

#include "Scene/SceneGraph.h"

#include <vector>

namespace
{
	// A root with childCount children linked directly, since Reparent walks every sibling
	std::vector<entt::entity> CreateWideHierarchy(entt::registry& registry, entt::entity root, int childCount)
	{
		std::vector<entt::entity> children;
		children.reserve(childCount);
		entt::entity previous = entt::null;
		for (int i = 0; i < childCount; i++)
		{
			entt::entity child = registry.create();
			registry.emplace<TransformComponent>(child, Vector3f(1.0f, 0.0f, 0.0f));
			HierarchyComponent& hierarchyComp = registry.emplace<HierarchyComponent>(child);
			hierarchyComp.parent = root;
			hierarchyComp.previousSibling = previous;
			if (previous != entt::null)
				registry.get<HierarchyComponent>(previous).nextSibling = child;
			else
				registry.get<HierarchyComponent>(root).firstChild = child;
			previous = child;
			children.push_back(child);
		}
		return children;
	}
}

TEST_CASE("Scene graph updates a parent with many children without running out of stack")
{
	entt::registry registry;
	entt::entity root = registry.create();
	registry.emplace<TransformComponent>(root, Vector3f(0.0f, 2.0f, 0.0f));
	registry.emplace<HierarchyComponent>(root);

	std::vector<entt::entity> children = CreateWideHierarchy(registry, root, 50000);

	SceneGraph::Traverse(registry);

	CHECK(registry.get<TransformComponent>(children.front()).GetWorldPosition() == Vector3f(1.0f, 2.0f, 0.0f));
	CHECK(registry.get<TransformComponent>(children.back()).GetWorldPosition() == Vector3f(1.0f, 2.0f, 0.0f));
}

TEST_CASE("Scene graph hides children of a hidden parent")
{
	entt::registry registry;
	entt::entity root = registry.create();
	registry.emplace<TransformComponent>(root);
	registry.emplace<HierarchyComponent>(root);
	std::vector<entt::entity> children = CreateWideHierarchy(registry, root, 3);

	SceneGraph::Traverse(registry);
	CHECK_FALSE(registry.all_of<HiddenInHierarchyMarker>(children[1]));

	registry.emplace<HiddenComponent>(root);
	SceneGraph::Traverse(registry);
	CHECK(registry.all_of<HiddenInHierarchyMarker>(root));
	CHECK(registry.all_of<HiddenInHierarchyMarker>(children[1]));

	registry.get<HiddenComponent>(root).hidden = false;
	registry.emplace<HiddenComponent>(children[1]);
	SceneGraph::Traverse(registry);
	CHECK_FALSE(registry.all_of<HiddenInHierarchyMarker>(children[0]));
	CHECK(registry.all_of<HiddenInHierarchyMarker>(children[1]));
}

TEST_CASE("Scene graph only recomputes transforms that changed, and their descendants")
{
	entt::registry registry;
	entt::entity root = registry.create();
	registry.emplace<TransformComponent>(root);
	registry.emplace<HierarchyComponent>(root);
	std::vector<entt::entity> children = CreateWideHierarchy(registry, root, 2);
	entt::entity other = registry.create();
	registry.emplace<TransformComponent>(other);

	SceneGraph::Traverse(registry);
	CHECK_FALSE(registry.get<TransformComponent>(root).IsDirty());
	CHECK_FALSE(registry.get<TransformComponent>(children[0]).IsDirty());

	// Setting the value it already has isn't a change
	registry.get<TransformComponent>(other).SetPosition(Vector3f(0.0f, 0.0f, 0.0f));
	CHECK_FALSE(registry.get<TransformComponent>(other).IsDirty());

	registry.get<TransformComponent>(root).SetPosition(Vector3f(0.0f, 3.0f, 0.0f));
	CHECK(registry.get<TransformComponent>(root).IsDirty());
	CHECK_FALSE(registry.get<TransformComponent>(children[0]).IsDirty());

	SceneGraph::UpdateWorldTransforms(registry);
	CHECK_FALSE(registry.get<TransformComponent>(root).IsDirty());
	CHECK(registry.get<TransformComponent>(children[1]).GetWorldPosition() == Vector3f(1.0f, 3.0f, 0.0f));
}

TEST_CASE("Scene graph brings one entity up to date from its highest changed ancestor")
{
	entt::registry registry;
	entt::entity root = registry.create();
	registry.emplace<TransformComponent>(root);
	registry.emplace<HierarchyComponent>(root);
	entt::entity child = CreateWideHierarchy(registry, root, 1).front();
	entt::entity grandchild = registry.create();
	registry.emplace<TransformComponent>(grandchild, Vector3f(1.0f, 0.0f, 0.0f));
	registry.emplace<HierarchyComponent>(grandchild).parent = child;
	registry.get<HierarchyComponent>(child).firstChild = grandchild;
	entt::entity other = registry.create();
	registry.emplace<TransformComponent>(other);

	SceneGraph::Traverse(registry);
	CHECK(registry.get<TransformComponent>(grandchild).GetWorldPosition() == Vector3f(2.0f, 0.0f, 0.0f));

	// The root changed but the child between them didn't
	registry.get<TransformComponent>(root).SetPosition(Vector3f(0.0f, 5.0f, 0.0f));
	registry.get<TransformComponent>(other).SetPosition(Vector3f(0.0f, 1.0f, 0.0f));
	SceneGraph::UpdateWorldTransform(registry, grandchild);

	CHECK(registry.get<TransformComponent>(grandchild).GetWorldPosition() == Vector3f(2.0f, 5.0f, 0.0f));
	CHECK_FALSE(registry.get<TransformComponent>(child).IsDirty());
	CHECK(registry.get<TransformComponent>(other).IsDirty());
}

TEST_CASE("A copied transform is recomputed")
{
	entt::registry registry;
	entt::entity entity = registry.create();
	TransformComponent& transformComp = registry.emplace<TransformComponent>(entity);
	SceneGraph::Traverse(registry);

	TransformComponent copy(Vector3f(4.0f, 0.0f, 0.0f));
	transformComp = copy;
	CHECK(transformComp.IsDirty());
	SceneGraph::Traverse(registry);
	CHECK(transformComp.GetWorldPosition() == Vector3f(4.0f, 0.0f, 0.0f));
}

TEST_CASE("Scene graph shows children again once their parent is no longer hidden")
{
	entt::registry registry;
	entt::entity root = registry.create();
	registry.emplace<TransformComponent>(root);
	registry.emplace<HierarchyComponent>(root);
	std::vector<entt::entity> children = CreateWideHierarchy(registry, root, 2);

	registry.emplace<HiddenComponent>(root);
	SceneGraph::Traverse(registry);
	CHECK(registry.all_of<HiddenInHierarchyMarker>(children[0]));

	registry.remove<HiddenComponent>(root);
	SceneGraph::Traverse(registry);
	CHECK_FALSE(registry.all_of<HiddenInHierarchyMarker>(root));
	CHECK_FALSE(registry.all_of<HiddenInHierarchyMarker>(children[0]));
	CHECK_FALSE(registry.all_of<HiddenInHierarchyMarker>(children[1]));
}
