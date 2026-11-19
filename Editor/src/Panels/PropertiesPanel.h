#pragma once

#include "HierarchyPanel.h"
#include "ImGui/ImGuiTilemapEditor.h"
#include "Asset/PhysicsMaterial.h"
#include "History/HistoryCommands.h"
#include "History/SceneHistory.h"
#include "Scene/Components.h"

#include "IconsFontAwesome6.h"

#include "imgui/imgui.h"

class PropertiesPanel :
	public Layer
{
public:
	PropertiesPanel(bool* show, Ref<HierarchyPanel> hierarchyPanel, Ref<TilemapEditor> tilemapEditor, Ref<SceneHistory> history);
	~PropertiesPanel() = default;

	void OnAttach() override;
	void OnUpdate(float deltaTime) override;
	void OnImGuiRender() override;
private:
	void DrawComponents(Entity entity);
	void DrawAddComponent(Entity entity);

	// A copy of the component to record an edit from, or nullptr while its step is open
	template<typename T>
	Scope<EditComponentCommand<T>> BeforeEdit(Entity entity)
	{
		if (!m_History->IsEnabled() || m_History->IsOpen(ComponentEditKey<T>(entity)))
			return nullptr;
		return CreateScope<EditComponentCommand<T>>(entity);
	}

	// Opens the step for an edit of the component, from the copy taken before the edit
	template<typename T>
	void RecordEdit(Entity entity, Scope<EditComponentCommand<T>>& before)
	{
		if (before)
			m_History->Begin(*entity.GetScene(), ComponentEditKey<T>(entity), std::move(before));
	}

	template<typename T>
	void AddComponentMenuItem(const char* name, Entity entity)
	{
		if (ImGui::MenuItem(name, nullptr, nullptr, !entity.HasComponent<T>()))
		{
			Scope<AddComponentCommand<T>> addComponentCommand = CreateScope<AddComponentCommand<T>>(entity);
			entity.AddComponent<T>();
			m_History->Push(*entity.GetScene(), std::move(addComponentCommand));
		}
	}

	template<typename T, typename UIFunction>
	void DrawComponent(const char* name, Entity entity, UIFunction uifunction, bool deletable = true)
	{
		if (entity.HasComponent<T>())
		{
			bool deleteComponent = false;

			if (ImGui::TreeNodeEx((void*)typeid(T).hash_code(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_FramePadding, "%s", name))
			{
				if (deletable)
				{
					if (ImGui::BeginPopupContextItem())
					{
						if (ImGui::MenuItem(ICON_FA_TRASH_CAN" Delete"))
						{
							deleteComponent = true;
						}

						ImGui::EndPopup();
					}
				}

				auto& component = entity.GetComponent<T>();

				uifunction(component);

				ImGui::TreePop();
			}

			if (deleteComponent) {
				Scope<RemoveComponentCommand<T>> removeComponentCommand = CreateScope<RemoveComponentCommand<T>>(entity);
				entity.RemoveComponent<T>();
				m_History->Push(*entity.GetScene(), std::move(removeComponentCommand));
			}
		}
	}

private:
	bool* m_Show;

	Ref<HierarchyPanel> m_HierarchyPanel;
	Ref<TilemapEditor> m_TilemapEditor;
	Ref<SceneHistory> m_History;

	Ref<Material> m_DefaultMaterial;
	Ref<PhysicsMaterial> m_DefaultPhysMaterial;

	std::vector<std::vector<uint32_t>> m_TilesBeforeResize;	// the tilemap's tiles while its size is edited
};