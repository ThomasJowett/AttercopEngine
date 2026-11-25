#pragma once

#include "Interfaces/ICopyable.h"
#include "History/SceneHistory.h"
#include "Selection/EntitySelection.h"
#include "Selection/SelectionOperations.h"

#include "TinyXml2/tinyxml2.h"
#include "Core/Layer.h"
#include "Scene/Entity.h"
#include "Renderer/Mesh.h"
#include "Asset/Material.h"

#include <optional>
#include <unordered_set>
#include <vector>

struct ImGuiTextFilter;
struct ImGuiMultiSelectIO;
class Scene;

class HierarchyPanel :
    public Layer, public ICopyable
{
public:
    HierarchyPanel(bool* show, Ref<SceneHistory> history, Ref<EntitySelection> selection);
    ~HierarchyPanel();

    void OnAttach() override;
    void OnDetach() override;
    void OnFixedUpdate() override;
    void OnImGuiRender() override;
    virtual void OnEvent(Event& event) override;

    // Inherited via ICopyable
    virtual void Copy() override;
    virtual void Cut() override;
    virtual void Paste() override;
    virtual void Duplicate() override;
    virtual void Delete() override;
    virtual bool HasSelection() const override;
    // With the filter active and this panel focused, every match; otherwise every entity
    virtual void SelectAll() override;
    virtual void SelectNone() override;
    virtual void InvertSelection() override;
    virtual bool IsReadOnly() const override;

    // Hides every selected entity, or shows them all if they're all hidden
    void ToggleHidden();

    bool IsFocused() { return m_Focused; }
    void HasFocused() { m_Focused = false; }
private:
    using DropPosition = SelectionOperations::DropPosition;

    struct Row
    {
        entt::entity entity;
        int depth;
        bool hasChildren;
    };

    void BuildRows(Scene* scene);
    void AddRows(entt::registry& registry, entt::entity entity, int depth);
    // The entities that pass the filter, in the order the rows list them
    std::vector<entt::entity> FilteredEntities(entt::registry& registry) const;
    // Applies the selection requests of a BeginMultiSelect or EndMultiSelect to the selection
    void ApplySelectionRequests(const ImGuiMultiSelectIO& io, Scene& scene);
    void DrawRow(Scene* scene, const Row& row, int index);
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

    Ref<EntitySelection> m_Selection;
    bool m_Focused = false;
    // The selection before a click, which a double-click on a selected row keeps
    SelectionState m_SelectionBeforeClick;
    bool m_SkipNextRelease = false;
    bool m_SkipSelectionRequests = false;

    Scope<ImGuiTextFilter> m_TextFilter;

    // Rows of the tree that are showing, rebuilt every frame
    std::vector<Row> m_Rows;
    // Nodes are open by default, so only the closed ones are stored
    std::unordered_set<entt::entity> m_ClosedNodes;

    Entity m_ContextEntity;
    // What AddCreated puts a new entity under; none for the root
    Entity m_CreateParent;
    // The row being dragged, kept submitted while it scrolls out of view
    Uuid m_DraggedRow = Uuid::Nil();
    bool m_OpenContextMenu = false;

    // Drops are applied after the rows are drawn
    struct PendingDrop
    {
        // The top-level entities being moved
        std::vector<Uuid> dragged;
        Entity target;
        DropPosition position;
    };
    std::optional<PendingDrop> m_PendingDrop;
};
