#pragma once

#include "HierarchyPanel.h"
#include "ComponentInspector.h"
#include "ImGui/ImGuiTilemapEditor.h"
#include "History/SceneHistory.h"

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

	bool* m_Show;

	Ref<HierarchyPanel> m_HierarchyPanel;
	Ref<TilemapEditor> m_TilemapEditor;
	Ref<SceneHistory> m_History;

	ComponentInspector m_Inspector;
};
