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

	// Opens the component's undo step before an edit that doesn't go through Set
	void RecordComponentEdit(Entity entity, const Reflection::ComponentType& type);

private:
	bool IsOn(Entity entity, const Reflection::PropertyPath& path) const;

	SceneHistory* m_History;
	uint32_t m_NextInteraction = 1;
	uint32_t m_Interaction = 0;
	std::optional<Uuid> m_Entity;
	Reflection::PropertyPath m_Path;
	// For EditFromStart fields: the component and the field's value when the interaction began
	std::optional<Reflection::ComponentValues> m_Start;
	std::optional<Reflection::Value> m_StartValue;
};
