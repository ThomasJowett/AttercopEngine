#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"

#include "ViewportCameraController.h"

#include "Interfaces/ICopyable.h"
#include "Interfaces/ISaveable.h"

#include "Asset/Shader.h"

#include "HierarchyPanel.h"
#include "ImGui/ImGuiTilemapEditor.h"
#include "History/HistoryCommands.h"
#include "History/SceneHistory.h"
#include "Selection/EntitySelection.h"
#include "Scene/Components/UIWidgets/WidgetComponent.h"

class ViewportPanel
	:public Layer, public ICopyable
{
	enum class OperationMode
	{
		Select = 0,
		Move,
		Rotate,
		Scale,
		Universal
	};

	enum class TranslationMode
	{
		Local,
		World
	};

public:
	ViewportPanel(bool* show, Ref<HierarchyPanel> hierarchyPanel, Ref<EntitySelection> selection, Ref<TilemapEditor> tilemapEditor, Ref<SceneHistory> history);
	~ViewportPanel() = default;

	void OnAttach() override;
	void OnDetach() override;
	void OnUpdate(float deltaTime) override;
	void OnImGuiRender() override;
	void OnEvent(Event& event) override;

	virtual void Copy() override;
	virtual void Cut() override;
	virtual void Paste() override;
	virtual void Duplicate() override;
	virtual void Delete() override;
	virtual void SelectAll() override;
	virtual void SelectNone() override;
	virtual void InvertSelection() override;
	virtual bool HasSelection() const override;
	virtual bool IsReadOnly() const override { return false; }

private:
	void HandleKeyboardInputs();

	// Bounds of what the entity draws, in its own space before its transform
	BoundingBox GetLocalBounds(Entity entity) const;
	// Frames the union of the selected entities' bounds
	void FrameSelectedEntities();
	// What frustum culling tests: one box per submesh for static meshes, otherwise one per entity
	void DrawBoundingBoxes();

private:
	bool* m_Show;
	bool m_WindowHovered = false;
	bool m_WindowFocussed = false;
	bool m_ViewportHovered = false;
	bool m_CursorDisabled = false;
	ImVec2 m_ViewportSize;
	Ref<FrameBuffer> m_Framebuffer;
	Ref<FrameBuffer> m_CameraPreview;
	Vector2f m_RelativeMousePosition;
	Vector2f m_MousePositionBeginClick;

	ViewportCameraController m_CameraController;

	OperationMode m_Operation;
	TranslationMode m_Translation;
	// Whether the gizmo drag in progress snaps rotation, in degrees, rather than units
	bool m_SnapInDegrees = false;
	bool m_Is2DMode = true;
	char m_GridAxis = 'z';

	bool m_ShowCollision = false;
	bool m_ShowFrameRate = true;
	bool m_ShowGrid = false;
	bool m_ShowStats = false;
	bool m_ShowBoundingBoxes = false;
	bool m_ShowShadows = true;
	bool m_ShowLighting = true;
	bool m_ShowReflections = true;

	int m_SelectedViewportShading = 0;

	// Copy, paste and the other selection commands go through the Hierarchy
	Ref<HierarchyPanel> m_HierarchyPanel;
	Ref<EntitySelection> m_Selection;
	Entity m_HoveredEntity;

	int m_PixelData = -1;

	Ref<TilemapEditor> m_TilemapEditor;
	bool m_RightClickMenuOpen = false;
	bool m_WidgetHandleActive = false;
	// The gizmo drag's undo interaction, 0 when it isn't in use
	uint32_t m_GizmoInteraction = 0;

	Ref<Mesh> m_GridMesh;

	Ref<SceneHistory> m_History;

	bool m_Fullscreen = false;
};