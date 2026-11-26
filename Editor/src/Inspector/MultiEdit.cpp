#include "MultiEdit.h"

#include "History/PropertyCommands.h"
#include "History/SceneHistory.h"
#include "Scene/Scene.h"

namespace MultiEdit
{
	namespace
	{
		void* TryGet(Entity entity, const Reflection::ComponentType& type)
		{
			return type.tryGet(entity.GetScene()->GetRegistry(), entity.GetHandle());
		}
	}

	ComponentSplit SplitComponents(const std::vector<Entity>& entities)
	{
		ComponentSplit split;
		if (entities.empty())
			return split;

		for (const Reflection::ComponentType* type : Reflection::Components())
		{
			size_t count = 0;
			for (Entity entity : entities)
			{
				if (TryGet(entity, *type))
					++count;
			}
			if (count == entities.size())
				split.common.push_back(type);
			else if (count > 0)
				split.partial.push_back(type);
		}
		return split;
	}

	std::optional<FieldState> ReadField(const std::vector<Entity>& entities, const Reflection::ComponentType& type, int field)
	{
		if (entities.empty())
			return std::nullopt;
		const Reflection::FieldInfo& info = type.fields[field];
		const void* first = TryGet(entities.front(), type);
		if (!first)
			return std::nullopt;

		FieldState state;
		state.value = info.get(first);
		const int parts = Reflection::PartCount(info.type);
		bool allMixed = false;
		for (size_t i = 1; i < entities.size(); ++i)
		{
			const void* component = TryGet(entities[i], type);
			if (!component)
				return std::nullopt;
			// Nothing more to find, but every entity must still have the component
			if (allMixed)
				continue;

			const Reflection::Value value = info.get(component);
			if (value == state.value)
				continue;
			state.mixed = true;
			allMixed = true;
			for (int part = 0; part < parts; ++part)
			{
				if (value.GetPart(part) != state.value.GetPart(part))
					state.partMixed[part] = true;
				allMixed = allMixed && state.partMixed[part];
			}
		}
		return state;
	}

	bool CanAdd(Entity entity, const Reflection::ComponentType& type)
	{
		return !TryGet(entity, type) && !Reflection::ExcludedBy(entity.GetScene()->GetRegistry(), entity.GetHandle(), type);
	}

	void AddComponent(SceneHistory* history, const std::vector<Entity>& entities, const Reflection::ComponentType& type,
		const std::function<void(Entity)>& add)
	{
		std::vector<Entity> targets;
		for (Entity entity : entities)
		{
			if (CanAdd(entity, type))
				targets.push_back(entity);
		}
		if (targets.empty())
			return;

		Scene& scene = *targets.front().GetScene();
		Scope<AddRemoveComponentCommand> command = CreateScope<AddRemoveComponentCommand>(targets, type,
			AddRemoveComponentCommand::Action::Add);
		for (Entity entity : targets)
			add(entity);
		scene.MakeDirty();
		if (history)
			history->Push(scene, std::move(command));
	}

	void RemoveComponent(SceneHistory* history, const std::vector<Entity>& entities, const Reflection::ComponentType& type)
	{
		std::vector<Entity> targets;
		for (Entity entity : entities)
		{
			if (TryGet(entity, type))
				targets.push_back(entity);
		}
		if (targets.empty())
			return;

		Scene& scene = *targets.front().GetScene();
		// An open edit of the component closes while it's still there
		if (history)
			history->End(scene);
		Scope<AddRemoveComponentCommand> command = CreateScope<AddRemoveComponentCommand>(targets, type,
			AddRemoveComponentCommand::Action::Remove);
		for (Entity entity : targets)
			type.remove(scene.GetRegistry(), entity.GetHandle());
		scene.MakeDirty();
		if (history)
			history->Push(scene, std::move(command));
	}
}
