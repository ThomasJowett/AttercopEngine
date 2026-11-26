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
#include "Selection/BoxSelect.h"
#include "Selection/EntitySelection.h"
#include "Selection/GizmoTransform.h"
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
	// The selected entity when exactly one is selected, for what only works on one
	Entity GetSingleSelected(Scene& scene) const;

	// Clicks select on release; a drag from empty space is a box select
	void HandleSelectionMouse(Scene& scene, const ImVec2& windowPos);
	// Every visible entity with a transform whose bounds touch the rectangle, in viewport pixels
	std::vector<Uuid> BoxSelectHits(Scene& scene, const Vector2f& corner, const Vector2f& otherCorner);
	// The bounds of each selected entity, the active one brighter, and their colliders
	void DrawSelectionOutlines(Scene& scene);
	void DrawColliderOutlines(Entity entity);
	// The gizmo moves a pivot, and each frame's change moves every top-level selected entity; view and projection are transposed
	void DrawGizmo(Scene& scene, const Matrix4x4& view, const Matrix4x4& projection);
	// The pivot matrix and, in Select mode, the selection's bounds around it
	void BuildGizmoPivot(Scene& scene, const std::vector<GizmoTransform::Item>& targets);
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
	GizmoTransform::Pivot m_Pivot = GizmoTransform::Pivot::Centre;
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
	// Drawn this frame, so ImGuizmo's hover state is current
	bool m_GizmoDrawn = false;
	Matrix4x4 m_GizmoPivot;
	// Select mode's bounds, relative to the pivot
	float m_GizmoBounds[6] = {};

	// A left press in the viewport that may become a click or a box select
	bool m_SelectPressed = false;
	bool m_BoxSelecting = false;
	Vector2f m_SelectPressPosition;
	bool m_SelectPressShift = false;
	bool m_SelectPressCtrl = false;
	// Escape puts this back
	SelectionState m_SelectionBeforeBox;

	Ref<Mesh> m_GridMesh;

	Ref<SceneHistory> m_History;

	bool m_Fullscreen = false;
};