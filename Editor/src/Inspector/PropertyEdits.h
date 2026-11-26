#pragma once

#include "Reflection/Reflection.h"
#include "Scene/Entity.h"

#include <cstdint>
#include <optional>
#include <vector>

class SceneHistory;

// Inspector edits through Reflection::Set, grouped into interactions from a widget's activation to its deactivation.
// Each edit applies to a list of entities as one step; one entity is a list of one
class PropertyEdits
{
public:
	// history may be nullptr, to record nothing
	explicit PropertyEdits(SceneHistory* history = nullptr) : m_History(history) {}

	// A widget became active: changes to this property on these entities until End share a new interaction number
	void Begin(const std::vector<Entity>& entities, const Reflection::PropertyPath& path);
	// Sets the value on every entity in the open interaction on this property, or one of its own; a path with a
	// part sets only that part. EditFromStart fields start from each entity's value at the start
	bool Set(const std::vector<Entity>& entities, const Reflection::PropertyPath& path, const Reflection::Value& value);
	void End();

	void Begin(Entity entity, const Reflection::PropertyPath& path) { Begin(std::vector<Entity>{ entity }, path); }
	bool Set(Entity entity, const Reflection::PropertyPath& path, const Reflection::Value& value)
	{
		return Set(std::vector<Entity>{ entity }, path, value);
	}

	bool IsOpen() const { return m_Interaction != 0; }
	uint32_t GetInteraction() const { return m_Interaction; }

	// Records the property for undo before an edit that doesn't go through Set, such as a custom field
	void Record(const std::vector<Entity>& entities, const Reflection::PropertyPath& path);
	// Records every saved field of the component before it's changed directly
	void RecordComponent(const std::vector<Entity>& entities, const Reflection::ComponentType& type);

	// A number no other interaction has, shared with the gizmo and the viewport's handles
	static uint32_t NewInteraction();

private:
	bool IsOn(const std::vector<Entity>& entities, const Reflection::PropertyPath& path) const;
	// The open interaction on this property, or a new one
	uint32_t InteractionFor(const std::vector<Entity>& entities, const Reflection::PropertyPath& path);

	SceneHistory* m_History;
	uint32_t m_Interaction = 0;
	std::vector<Uuid> m_Entities;
	// The whole field, without a part
	Reflection::PropertyPath m_Path;
	// For EditFromStart fields: each entity's component and field value when the interaction began
	std::vector<Reflection::ComponentValues> m_Start;
	std::vector<std::optional<Reflection::Value>> m_StartValues;
};
