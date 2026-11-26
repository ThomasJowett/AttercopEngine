#pragma once

#include "Selection/EntitySelection.h"
#include "ComponentInspector.h"
#include "ImGui/ImGuiTilemapEditor.h"
#include "History/PropertyCommands.h"
#include "History/SceneHistory.h"

#include <filesystem>
#include <vector>

class PropertiesPanel :
	public Layer
{
public:
	PropertiesPanel(bool* show, Ref<EntitySelection> selection, Ref<TilemapEditor> tilemapEditor, Ref<SceneHistory> history);
	~PropertiesPanel() = default;

	void OnImGuiRender() override;
private:
	// The editor's own drawing for a few components and fields
	void RegisterOverrides();
	// The selected entities that are in the scene, the active one first
	std::vector<Entity> SelectedEntities(Scene& scene) const;
	// A dropped file into the component's asset field on every entity, adding the component where it's missing, as one undo step
	void SetOrAdd(const std::vector<Entity>& entities, const Reflection::ComponentType& type, const char* field, const std::filesystem::path& file);
	void SetSceneSetting(Scene& scene, SetSceneSettingCommand::Setting setting, const Reflection::Value& value);

	bool* m_Show;

	Ref<EntitySelection> m_Selection;
	Ref<TilemapEditor> m_TilemapEditor;
	Ref<SceneHistory> m_History;

	ComponentInspector m_Inspector;
};
