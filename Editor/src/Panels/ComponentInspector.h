#pragma once

#include "Inspector/MultiEdit.h"
#include "Inspector/PropertyEdits.h"
#include "Reflection/Reflection.h"
#include "Scene/Entity.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class SceneHistory;

// Draws any described component for the selected entities: a header per component they all have, a widget per field,
// every edit through Reflection::Set on all of them. The first entity is the active one; one entity is a list of one
class ComponentInspector
{
public:
	using Entities = std::vector<Entity>;
	using ComponentDrawer = std::function<void(ComponentInspector& inspector, const Entities& entities)>;
	using FieldDrawer = std::function<void(ComponentInspector& inspector, const Entities& entities, const Reflection::FieldInfo& field)>;

	// An override that edits one entity is given only the active one, read-only, when several are selected
	enum class Support { OneEntity, ManyEntities };

	// Shown for a value that differs between the entities
	static constexpr const char* MixedText = "--";

	explicit ComponentInspector(SceneHistory* history);

	// Draws the component, by element name, instead of its fields
	void ReplaceComponent(std::string_view component, Support support, ComponentDrawer draw);
	// Draws after the component's fields
	void AppendToComponent(std::string_view component, Support support, ComponentDrawer draw);
	// Draws one field, by path, "StaticMesh.Materials"
	void ReplaceField(std::string_view path, Support support, FieldDrawer draw);

	// With several entities, how many there are and how many components are hidden as not all of them have them
	void DrawSummary(const Entities& entities);
	// A header for each component every entity has that the inspector shows, in registration order
	void Draw(const Entities& entities);
	// The component's fields, for replaced components that draw them too
	void DrawFields(const Entities& entities, const Reflection::ComponentType& type);
	// One field, wherever it's shown; false if it isn't
	bool DrawField(const Entities& entities, const Reflection::ComponentType& type, int field);
	// Offers the components at least one entity lacks
	void DrawAddComponentMenu(const Entities& entities);
	// Adds the component with add to each entity that lacks it, as one undo step that keeps the values add gave it
	void AddComponent(const Entities& entities, const Reflection::ComponentType& type, const std::function<void(Entity)>& add);

	// Sets a property on every entity as an inspector edit, recorded for undo
	bool Set(const Entities& entities, const Reflection::PropertyPath& path, const Reflection::Value& value);
	PropertyEdits& GetEdits() { return m_Edits; }

	// Whether the inspector and the Add Component menu show the component
	static bool IsShown(const Reflection::ComponentType& type);
	// Icon and name for headers and the menu
	static std::string Title(const Reflection::ComponentType& type);

private:
	// A value from a widget, for the whole field or one part of it
	struct Edit
	{
		Reflection::Value value;
		int part = -1;
	};

	template<typename Drawer>
	struct Override
	{
		Support support;
		Drawer draw;
	};

	// The entities' common and partial components, kept while the recorded state and entities are the same
	const MultiEdit::ComponentSplit& Split(const Entities& entities);
	void DrawComponent(const Entities& entities, const Reflection::ComponentType& type);
	// Calls draw with the entities, or with the active one read-only when it edits one at a time
	void DrawOverride(Support support, const Entities& entities, const std::function<void(const Entities&)>& draw);
	// The edited value, if the widget changed it; many is set when several entities are shown
	std::optional<Edit> DrawWidget(const Reflection::FieldInfo& field, const MultiEdit::FieldState& state,
		const Reflection::Value& defaultValue, const void* component, bool many);
	Reflection::Value DefaultOf(const Reflection::ComponentType& type, int field);

	PropertyEdits m_Edits;
	SceneHistory* m_History;

	std::unordered_map<std::string, Override<ComponentDrawer>> m_ReplacedComponents;
	std::unordered_map<std::string, Override<ComponentDrawer>> m_Appended;
	std::unordered_map<std::string, Override<FieldDrawer>> m_ReplacedFields;

	struct SplitCache
	{
		Entities entities;
		uint64_t stateId = 0;
		bool valid = false;
		MultiEdit::ComponentSplit split;
	};
	SplitCache m_SplitCache;

	// A default-constructed component per type, for right-click resets
	std::unordered_map<const Reflection::ComponentType*, std::shared_ptr<void>> m_Defaults;
	// Fields whose parts are locked together, unlocked when present
	std::unordered_map<const Reflection::FieldInfo*, bool> m_Unlocked;
};
