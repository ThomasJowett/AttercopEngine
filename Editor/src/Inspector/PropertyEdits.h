#pragma once

#include "Reflection/Reflection.h"
#include "Scene/Entity.h"

#include <cstdint>
#include <optional>

class SceneHistory;

// Inspector edits through Reflection::Set, grouped into interactions from a widget's activation to its deactivation
class PropertyEdits
{
public:
	// history may be nullptr, to record nothing
	explicit PropertyEdits(SceneHistory* history = nullptr) : m_History(history) {}

	// A widget became active: changes to this property until End share a new interaction number
	void Begin(Entity entity, const Reflection::PropertyPath& path);
	// Sets the value in the open interaction on this property, or one of its own; EditFromStart fields start from its start
	bool Set(Entity entity, const Reflection::PropertyPath& path, const Reflection::Value& value);
	void End();

	bool IsOpen() const { return m_Interaction != 0; }
	uint32_t GetInteraction() const { return m_Interaction; }

	// Records the property for undo before an edit that doesn't go through Set, such as a custom field
	void Record(Entity entity, const Reflection::PropertyPath& path);
	// Records every saved field of the component before it's changed directly
	void RecordComponent(Entity entity, const Reflection::ComponentType& type);

	// A number no other interaction has, shared with the gizmo and the viewport's handles
	static uint32_t NewInteraction();

private:
	bool IsOn(Entity entity, const Reflection::PropertyPath& path) const;
	// The open interaction on this property, or a new one
	uint32_t InteractionFor(Entity entity, const Reflection::PropertyPath& path);

	SceneHistory* m_History;
	uint32_t m_Interaction = 0;
	std::optional<Uuid> m_Entity;
	Reflection::PropertyPath m_Path;
	// For EditFromStart fields: the component and the field's value when the interaction began
	std::optional<Reflection::ComponentValues> m_Start;
	std::optional<Reflection::Value> m_StartValue;
};
