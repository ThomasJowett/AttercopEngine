#pragma once

#include "HierarchyPanel.h"
#include "ComponentInspector.h"
#include "ImGui/ImGuiTilemapEditor.h"
#include "History/PropertyCommands.h"
#include "History/SceneHistory.h"

#include <filesystem>

class PropertiesPanel :
	public Layer
{
public:
	PropertiesPanel(bool* show, Ref<HierarchyPanel> hierarchyPanel, Ref<TilemapEditor> tilemapEditor, Ref<SceneHistory> history);
	~PropertiesPanel() = default;

	void OnImGuiRender() override;
private:
	// The editor's own drawing for a few components and fields
	void RegisterOverrides();
	// A dropped file into the component's asset field, adding the component if it's missing, as one undo step
	void SetOrAdd(Entity entity, const Reflection::ComponentType& type, const char* field, const std::filesystem::path& file);
	void SetSceneSetting(Scene& scene, SetSceneSettingCommand::Setting setting, const Reflection::Value& value);

	bool* m_Show;

	Ref<HierarchyPanel> m_HierarchyPanel;
	Ref<TilemapEditor> m_TilemapEditor;
	Ref<SceneHistory> m_History;

	ComponentInspector m_Inspector;
};
