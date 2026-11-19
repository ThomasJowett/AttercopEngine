#pragma once

#include "Interfaces/ICopyable.h"
#include "History/SceneHistory.h"

#include "TinyXml2/tinyxml2.h"
#include "Core/Layer.h"
#include "Scene/Entity.h"
#include "Renderer/Mesh.h"
#include "Asset/Material.h"

#include <optional>
#include <unordered_set>
#include <vector>

struct ImGuiTextFilter;
class Scene;

class HierarchyPanel :
    public Layer, public ICopyable
{
public:
    HierarchyPanel(bool* show, Ref<SceneHistory> history);
    ~HierarchyPanel();

    void OnAttach() override;
    void OnDetach() override;
    void OnFixedUpdate() override;
    void OnImGuiRender() override;
    virtual void OnEvent(Event& event) override;

    Entity GetSelectedEntity();
    void SetSelectedEntity(Entity entity);

    // Inherited via ICopyable
    virtual void Copy() override;
    virtual void Cut() override;
    virtual void Paste() override;
    virtual void Duplicate() override;
    virtual void Delete() override;
    virtual bool HasSelection() const override;
    virtual void SelectAll() override;
    virtual bool IsReadOnly() const override;

    bool IsFocused() { return m_Focused; }
    void HasFocused() { m_Focused = false; }
private:
    enum class DropPosition { Before, Onto, After };

    struct Row
    {
        entt::entity entity;
        int depth;
        bool hasChildren;
    };

    void BuildRows(Scene* scene);
    void AddRows(entt::registry& registry, entt::entity entity, int depth);
    void DrawRow(Scene* scene, const Row& row);
    void RowDragDropTarget(Entity entity, bool canDropAfter);
    void DragDropTarget(Entity parent);
    void ApplyDrop();
    void EntityContextMenu();

    void CreateMenu();
    // Puts a new entity under the selection, records it and selects it
    void AddCreated(Entity entity);
private:
    bool* m_Show;
    Ref<SceneHistory> m_History;

    Entity m_SelectedEntity;
    bool m_Focused = false;

    Scope<ImGuiTextFilter> m_TextFilter;

    // Rows of the tree that are showing, rebuilt every frame
    std::vector<Row> m_Rows;
    // Nodes are open by default, so only the closed ones are stored
    std::unordered_set<entt::entity> m_ClosedNodes;

    Entity m_ContextEntity;
    bool m_OpenContextMenu = false;

    // Drops are applied after the rows are drawn
    struct PendingDrop
    {
        Entity dragged;
        Entity target;
        DropPosition position;
    };
    std::optional<PendingDrop> m_PendingDrop;
};
