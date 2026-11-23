#pragma once

#include "Inspector/PropertyEdits.h"
#include "Reflection/Reflection.h"
#include "Scene/Entity.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

class SceneHistory;

// Draws any described component: a header per component, a widget per field, every edit through Reflection::Set
class ComponentInspector
{
public:
	using ComponentDrawer = std::function<void(ComponentInspector& inspector, Entity entity)>;
	using FieldDrawer = std::function<void(ComponentInspector& inspector, Entity entity, const Reflection::FieldInfo& field)>;

	explicit ComponentInspector(SceneHistory* history);

	// Draws the component, by element name, instead of its fields
	void ReplaceComponent(std::string_view component, ComponentDrawer draw);
	// Draws after the component's fields
	void AppendToComponent(std::string_view component, ComponentDrawer draw);
	// Draws one field, by path, "StaticMesh.Materials"
	void ReplaceField(std::string_view path, FieldDrawer draw);

	// A header for each component the entity has that the inspector shows, in registration order
	void Draw(Entity entity);
	// The component's fields, for replaced components that draw them too
	void DrawFields(Entity entity, const Reflection::ComponentType& type);
	// One field, wherever it's shown; false if it isn't
	bool DrawField(Entity entity, const Reflection::ComponentType& type, int field);
	void DrawAddComponentMenu(Entity entity);
	// Adds the component with add, as one undo step that keeps the values add gave it
	void AddComponent(Entity entity, const Reflection::ComponentType& type, const std::function<void()>& add);

	// Sets a property as an inspector edit, recorded for undo
	bool Set(Entity entity, const Reflection::PropertyPath& path, const Reflection::Value& value);
	PropertyEdits& GetEdits() { return m_Edits; }

	// Whether the inspector and the Add Component menu show the component
	static bool IsShown(const Reflection::ComponentType& type);
	// Icon and name for headers and the menu
	static std::string Title(const Reflection::ComponentType& type);

private:
	void DrawComponent(Entity entity, const Reflection::ComponentType& type);
	// The edited value, if the widget changed it
	std::optional<Reflection::Value> DrawWidget(const Reflection::FieldInfo& field, const Reflection::Value& value,
		const Reflection::Value& defaultValue, const void* component);
	Reflection::Value DefaultOf(const Reflection::ComponentType& type, int field);

	PropertyEdits m_Edits;
	SceneHistory* m_History;

	std::unordered_map<std::string, ComponentDrawer> m_ReplacedComponents;
	std::unordered_map<std::string, ComponentDrawer> m_Appended;
	std::unordered_map<std::string, FieldDrawer> m_ReplacedFields;

	// A default-constructed component per type, for right-click resets
	std::unordered_map<const Reflection::ComponentType*, std::shared_ptr<void>> m_Defaults;
	// Fields whose parts are locked together, unlocked when present
	std::unordered_map<const Reflection::FieldInfo*, bool> m_Unlocked;
};
